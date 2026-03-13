#include "server.h"
#include "admin.h"
#include "ai.h"
#include "config.h"
#include "engine.h"
#include "transformer.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
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

void engine_start_multiplexed(NodeConfig *config, int outbound_socket) {
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

  if (outbound_socket >= 0) {
    set_non_blocking(outbound_socket);
    SocketContext *outbound_ctx = malloc(sizeof(SocketContext));
    outbound_ctx->fd = outbound_socket;
    outbound_ctx->type = SOCKET_SWARM_PEER;

    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = outbound_ctx;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, outbound_socket, &ev);

    printf("[Engine] Now monitoring outbound connection on socket %d\n",
           outbound_socket);

    // We add them to our book (we know their FD, but not necessarily their
    // listener port yet, so we use 0)
    add_peer_to_swarm_V2(outbound_socket, 0);

    // --- THE NEW HANDSHAKE FIRE ---
    SwarmHandshake shake;
    shake.packet_type = PACKET_TYPE_HANDSHAKE;
    shake.listen_port =
        config->port; // Tell the seed node what port WE are listening on!

    send(outbound_socket, &shake, sizeof(SwarmHandshake), 0);
    printf("[PEX] Handshake fired! Told Seed we are listening on Port %d\n",
           config->port);
  }

  // 3. THE MULTIPLEXING EVENT LOOP
  while (keep_running) {
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
        add_peer_to_swarm_V2(client_fd, config->port);

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
        uint8_t raw_buffer[1024];
        memset(raw_buffer, 0, sizeof(raw_buffer));
        int bytes_read = read(ctx->fd, raw_buffer, sizeof(raw_buffer));

        if (bytes_read > 0) {
          uint8_t incoming_type = raw_buffer[0];

          // ==========================================
          // ROUTE A: WE RECEIVED A HANDSHAKE
          // ==========================================
          if (incoming_type == PACKET_TYPE_HANDSHAKE) {
            SwarmHandshake *shake = (SwarmHandshake *)raw_buffer;
            printf("\n[PEX] Caught Handshake! Peer on fd %d is listening on "
                   "Port %d\n",
                   ctx->fd, shake->listen_port);

            // 1. Update our book
            for (int p = 0; p < peer_count; p++) {
              if (connected_peers[p].fd == ctx->fd) {
                connected_peers[p].listen_port = shake->listen_port;
                printf("[PEX] Address Book updated for fd %d!\n", ctx->fd);
                break;
              }
            }

            // 2. THE SEED REPLY: Build a list of all OTHER peers we know about
            PeerListPacket reply;
            memset(&reply, 0, sizeof(PeerListPacket));
            reply.packet_type = PACKET_TYPE_PEER_LIST;

            for (int p = 0; p < peer_count; p++) {
              // Don't send the new guy his own port, and skip Port 0
              // (uninitialized)
              if (connected_peers[p].fd != ctx->fd &&
                  connected_peers[p].listen_port > 0) {
                reply.peer_ports[reply.num_peers] =
                    connected_peers[p].listen_port;
                reply.num_peers++;
                if (reply.num_peers >= 10)
                  break; // Cap at 10 for safety
              }
            }

            // 3. Fire the book back to the new node!
            if (reply.num_peers > 0) {
              send(ctx->fd, &reply, sizeof(PeerListPacket), 0);
              printf("[PEX] Sent Peer List (%d peers) back to fd %d!\n\n",
                     reply.num_peers, ctx->fd);
            } else {
              printf("[PEX] No other peers to share yet.\n\n");
            }
          } // ==========================================
            // ROUTE C: WE RECEIVED A PEER LIST
            // ==========================================
          // ==========================================
          // ROUTE C: WE RECEIVED A PEER LIST
          // ==========================================
          else if (incoming_type == PACKET_TYPE_PEER_LIST) {
            PeerListPacket *list = (PeerListPacket *)raw_buffer;
            printf("\n[PEX] Caught Peer List! Seed gave us %d ports to dial.\n",
                   list->num_peers);

            for (int i = 0; i < list->num_peers; i++) {
              int target_port = list->peer_ports[i];

              // 1. Don't dial ourselves!
              if (target_port == config->port)
                continue;

              // 2. Don't dial people we already know!
              int already_know = 0;
              for (int p = 0; p < peer_count; p++) {
                if (connected_peers[p].listen_port == target_port) {
                  already_know = 1;
                  break;
                }
              }
              if (already_know)
                continue;

              printf("[PEX] -> Auto-Dialing new Swarm friend on Port: %d...\n",
                     target_port);

              // 3. Dial them!
              int new_fd = socket(AF_INET, SOCK_STREAM, 0);
              struct sockaddr_in addr;
              memset(&addr, 0, sizeof(addr)); // CRITICAL: Clean the memory!

              addr.sin_family = AF_INET;
              addr.sin_port = htons(target_port);
              addr.sin_addr.s_addr = inet_addr("127.0.0.1");

              if (connect(new_fd, (struct sockaddr *)&addr, sizeof(addr)) ==
                  0) {
                set_non_blocking(new_fd);

                SocketContext *new_ctx = malloc(sizeof(SocketContext));
                new_ctx->fd = new_fd;
                new_ctx->type = SOCKET_SWARM_PEER;

                struct epoll_event new_ev;
                new_ev.events = EPOLLIN | EPOLLET;
                new_ev.data.ptr = new_ctx;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &new_ev);

                // Add them to the book!
                add_peer_to_swarm_V2(new_fd, target_port);

                // 4. Fire our handshake to the new friend!
                SwarmHandshake shake;
                shake.packet_type = PACKET_TYPE_HANDSHAKE;
                shake.listen_port = config->port;
                send(new_fd, &shake, sizeof(SwarmHandshake), 0);

                printf("[PEX] Successfully connected and handshook Port %d!\n",
                       target_port);
              } else {
                printf("[-] Auto-Dial failed for Port %d\n", target_port);
                close(new_fd);
              }
            }
            printf("\n");
          } else if (incoming_type == PACKET_TYPE_THOUGHT) {
            // Cast the raw bytes into our AISignal struct
            AISignal *incoming_thought_ptr = (AISignal *)raw_buffer;

            // Copy it into a local struct so we don't mess with the raw buffer
            AISignal incoming_thought = *incoming_thought_ptr;

            // --- SHIELD 1: THE BREADCRUMB CACHE ---
            if (protocol_is_duplicate(incoming_thought.message_id)) {
              printf("[Net] Echo detected (MsgID: %u). Dropping thought.\n",
                     incoming_thought.message_id);
              continue; // Skip the rest of the loop!
            }

            // --- SHIELD 2: ENERGY (Time-To-Live) ---
            if (incoming_thought.energy <= 0) {
              printf(
                  "[Net] Thought (MsgID: %u) ran out of energy. Dissipating.\n",
                  incoming_thought.message_id);
              continue; // Skip the rest of the loop!
            }

            // --- 1. RECORD THE BREADCRUMB ---
            // Save it so we don't process it again if it loops back!
            protocol_save_breadcrumb(incoming_thought.message_id, ctx->fd);

            // --- 2. PROCESS THE THOUGHT ---
            // ... inside server.c epoll read loop ...

            // --- 2. PROCESS THE THOUGHT ---
            printf("\n[+] --- INCOMING THOUGHT --- \n");

            // THE UPGRADE: Decode the tensor back to English to print it!
            char decoded_text[512];
            tensor_to_text(incoming_thought.tensor_data, decoded_text,
                           sizeof(decoded_text));

            printf("[Net] MsgID: %u | Energy: %d\n",
                   incoming_thought.message_id, incoming_thought.energy);
            printf("[Net] Translated Thought: %s\n", decoded_text);

            // ... relay to other peers ...

            AISignal network_response;
            ai_process_forward_signal(&incoming_thought, &network_response);

            // --- 3. RELAY TO THE SWARM (The Middleman upgrade) ---
            incoming_thought.energy -= 1; // It loses 1 energy per hop

            if (incoming_thought.energy > 0) {
              printf(
                  "[Net] Relaying thought to other peers with Energy: %d...\n",
                  incoming_thought.energy);
              ai_node_broadcast(&incoming_thought);
            }
          }
        } else if (bytes_read == 0) {
          // The other node shut down or disconnected
          printf("[-] Swarm Peer on fd %d disconnected.\n", ctx->fd);

          // --- THE GHOST BUSTER ---
          // Scrub them from the Address Book so we don't share dead ports!
          for (int p = 0; p < peer_count; p++) {
            if (connected_peers[p].fd == ctx->fd) {
              connected_peers[p].listen_port = 0; // Mark as dead
              connected_peers[p].fd = -1;
              break;
            }
          }

          close(ctx->fd);
          free(ctx);
        }

      } else if (ctx->type == SOCKET_ADMIN_CLIENT) {
        // Text/Commands from a human!
        int client_fd = ctx->fd;

        // 1. Create a clean buffer for the JSON
        char buffer[2048];
        memset(buffer, 0, sizeof(buffer));

        // 2. Read the data directly!
        // (Because epoll only triggered this because data arrived, we don't
        // even need the sledgehammer loop, but we will keep a small wait just
        // to be safe).
        int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read <=
            0) { // If it failed on the first try, try the sledgehammer
          for (int attempts = 0; attempts < 50; attempts++) {
            usleep(10000);
            bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0)
              break;
          }
        }

        if (bytes_read > 0) {
          // 3. Cap the string so the JSON parser doesn't crash
          buffer[bytes_read] = '\0';

          // 4. Pass the string, the fd, and the config!
          process_admin_command(buffer, client_fd, config);
        } else {
          printf("[-] Admin socket empty or disconnected. (Bytes read: %d)\n",
                 bytes_read);
          close(client_fd);
        }

        // CRITICAL: Clean up the memory epoll used for this interaction!
        // (Assuming process_admin_command already closes the client_fd)
        free(ctx);
      }
    }
  }
}
