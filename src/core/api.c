#include "api.h"
#include "admin.h"
#include "client.h"
#include "config.h"
#include "engine.h"
#include "server.h"
#include "threadpool.h"
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

// Global pointer to store the user's callback function
on_receive_func app_callback = NULL;

void ai_node_set_callback(on_receive_func callback) {
  app_callback = callback;
  printf("[API] Application callback registered successfully.\n");
}

void *admin_airlock_loop(void *arg) {
  NodeConfig *config = (NodeConfig *)arg;

  // We reuse your setup function, but for the admin_port!
  int admin_fd = setup_listening_socket(config->admin_port);
  if (admin_fd < 0) {
    printf("[!] Failed to bind Admin Airlock on port %d\n", config->admin_port);
    return NULL;
  }

  printf("[Admin] Airlock active on port %d (Dedicated Thread)\n",
         config->admin_port);

  while (1) {
    // This thread will just sleep here until Python connects
    int client_fd = accept(admin_fd, NULL, NULL);
    if (client_fd < 0)
      continue;

    AdminCommand cmd;
    int bytes_read = read(client_fd, &cmd, sizeof(AdminCommand));

    if (bytes_read == sizeof(AdminCommand)) {
      // Drop the pebble into the AI!
      process_admin_command(client_fd, &cmd, config);
    } else {
      printf("[Admin] Malformed payload received. Dropping.\n");
    }

    close(client_fd); // Hang up the phone after one command
  }
  return NULL;
}

void ai_node_start(int my_port, const char *peer_ip, int peer_port,
                   int num_threads, int queue_limit, NodeConfig *config) {
  int outbound_socket = -1;

  pthread_t admin_thread_id;
  if (pthread_create(&admin_thread_id, NULL, admin_airlock_loop, config) != 0) {
    printf("[!] Failed to spawn Admin thread.\n");
  }

  if (peer_ip != NULL && peer_port > 0) {
    outbound_socket = connect_to_node(peer_ip, peer_port);
  }

  ThreadPool *pool = threadpool_create(num_threads, queue_limit);
  int master_socket = setup_listening_socket(my_port);

  // This will now block UNTIL keep_running is set to 0
  run_epoll_engine(master_socket, outbound_socket, my_port, pool);
  printf("[Engine] Booting classic Swarm Engine on port %d...\n", config->port);

  // WHEN IT UNBLOCKS, CLEAN UP!
  threadpool_destroy(pool);
  printf("[*] Node successfully shut down. Goodbye!\n");
}

int ai_node_send(int target_fd, char *data, int size) {
  // A simple wrapper around the raw socket send.
  // In a production system, you'd wrap this in a while loop to handle EAGAIN
  int sent = send(target_fd, data, size, 0);
  if (sent < 0) {
    perror("[API] Send failed");
  }
  return sent;
}
