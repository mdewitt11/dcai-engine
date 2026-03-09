#include "server.h"
#include "admin.h"
#include "config.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 1000

// Helper to make sockets non-blocking
void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 1. Sets a socket to non-blocking mode so it doesn't freeze the engine
int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 2. Creates, binds, and starts listening on a specific port
int setup_listening_socket(int port) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
    return -1;

  // Prevent "Address already in use" errors if you restart the node quickly
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(server_fd);
    return -1;
  }

  if (listen(server_fd, SOMAXCONN) < 0) {
    close(server_fd);
    return -1;
  }

  return server_fd;
}

// Helper to bind a listening port
int create_listener(int port) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("failed to bind");
  }
  listen(server_fd, SOMAXCONN);
  set_non_blocking(server_fd);

  return server_fd;
}

void engine_start_multiplexed(NodeConfig *config) {
  int epoll_fd = epoll_create1(0);
  struct epoll_event ev, events[MAX_EVENTS];

  // 1. BOOT THE SWARM PORT (8000)
  SocketContext *swarm_ctx = malloc(sizeof(SocketContext));
  swarm_ctx->fd = create_listener(config->port);
  swarm_ctx->type = SOCKET_SWARM_LISTENER;

  ev.events = EPOLLIN;
  ev.data.ptr = swarm_ctx;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, swarm_ctx->fd, &ev);
  printf("[Engine] Swarm Port %d bound and listening.\n", config->port);

  // 2. BOOT THE ADMIN AIRLOCK (8080)
  SocketContext *admin_ctx = malloc(sizeof(SocketContext));
  admin_ctx->fd = create_listener(config->admin_port);
  admin_ctx->type = SOCKET_ADMIN_LISTENER;

  ev.events = EPOLLIN;
  ev.data.ptr = admin_ctx;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, admin_ctx->fd, &ev);
  printf("[Engine] Admin Airlock %d bound and listening.\n",
         config->admin_port);

  // 3. THE MULTIPLEXING EVENT LOOP
  while (1) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

    for (int i = 0; i < nfds; i++) {
      SocketContext *ctx = (SocketContext *)events[i].data.ptr;

      // --- HANDLING NEW CONNECTIONS ---
      if (ctx->type == SOCKET_SWARM_LISTENER) {
        // A new AI node is connecting to us!
        int client_fd = accept(ctx->fd, NULL, NULL);
        set_non_blocking(client_fd);

        SocketContext *peer_ctx = malloc(sizeof(SocketContext));
        peer_ctx->fd = client_fd;
        peer_ctx->type = SOCKET_SWARM_PEER;

        ev.events = EPOLLIN | EPOLLET; // Edge-triggered for high speed
        ev.data.ptr = peer_ctx;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
        printf("[Engine] New Swarm Peer connected on fd %d\n", client_fd);

      } else if (ctx->type == SOCKET_ADMIN_LISTENER) {
        // A human script is connecting to the airlock!
        int client_fd = accept(ctx->fd, NULL, NULL);
        set_non_blocking(client_fd);

        SocketContext *client_ctx = malloc(sizeof(SocketContext));
        client_ctx->fd = client_fd;
        client_ctx->type = SOCKET_ADMIN_CLIENT;

        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = client_ctx;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
        printf("[Engine] Admin Script connected on fd %d\n", client_fd);

        // --- HANDLING INCOMING DATA ---
      } else if (ctx->type == SOCKET_SWARM_PEER) {
        // Raw binary from the swarm! Pass to threadpool ->
        // incoming_data_handler
        // TODO: Read into AISignal buffer

      } else if (ctx->type == SOCKET_ADMIN_CLIENT) {
        // Text/Commands from a human!
        AdminCommand cmd;
        int bytes_read = read(ctx->fd, &cmd, sizeof(AdminCommand));

        if (bytes_read == sizeof(AdminCommand)) {
          // Pass it to the airlock logic we wrote earlier
          process_admin_command(ctx->fd, &cmd, config);
        } else {
          printf("[!] Malformed admin packet. Dropping.\n");
        }

        // Admin scripts usually disconnect after one command, so we clean up
        close(ctx->fd);
        free(ctx);
      }
    }
  }
}
