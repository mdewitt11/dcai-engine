#include "api.h"
#include "admin.h"
#include "client.h"
#include "config.h"
#include "engine.h"
#include "server.h"
#include "threadpool.h"
#include <stdio.h>
#include <string.h>
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
    if (client_fd >= 0) {

      // 1. Create a clean buffer for the JSON
      char buffer[2048];
      memset(buffer, 0, sizeof(buffer));

      // 2. Read the raw JSON from Python using YOUR client_fd
      int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

      if (bytes_read > 0) {
        // 3. Cap the string so the JSON parser doesn't crash
        buffer[bytes_read] = '\0';

        // 4. Pass the string, the fd, and the config!
        process_admin_command(buffer, client_fd, config);
      } else {
        close(client_fd);
      }
    }
    return NULL;
  }
}

void ai_node_start(NodeConfig *config) {
  int outbound_socket = -1;

  ThreadPool *pool =
      threadpool_create(config->num_threads, config->max_queue_size);
  if (!pool) {
    printf("[!] Failed to initialize ThreadPool.\n");
    return;
  }

  if (config->target_ip != '\0' && strlen(config->target_ip) > 0 &&
      config->target_port > 0) {
    printf("[*] Attempting to connect to %s:%d...\n", config->target_ip,
           config->target_port);

    outbound_socket = connect_to_node(config->target_ip, config->target_port);

    if (outbound_socket >= 0) {
      printf("[+] Successfully connected to Node at %s:%d!\n",
             config->target_ip, config->target_port);
    } else {
      printf("[-] Failed to connect to seed node. Booting isolated.\n");
    }
  }

  printf("[Engine] Booting Multiplexed Engine...\n");
  engine_start_multiplexed(config, outbound_socket);

  // WHEN IT UNBLOCKS (Ctrl+C), CLEAN UP!
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
