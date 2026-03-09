#include "ai.h" // NEW: For the AI brain
#include "api.h"
#include "config.h"
#include "protocol.h" // NEW: For the routing cache
#include <signal.h>
#include <stdio.h>
#include <string.h>

volatile sig_atomic_t keep_running = 1;
char my_node_id[128];

void handle_shutdown_signal(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    printf("\n[!] Shutdown signal received! Commencing graceful exit...\n");
    keep_running = 0;
  }
}

// --- THE BRIDGE: Network to Brain ---
void incoming_data_handler(int client_fd, char *data, int size) {
  // 1. Safety Check: Is this a complete AI Signal?
  if (size != sizeof(AISignal)) {
    printf("[!] Warning: Received malformed packet of size %d. Dropping.\n",
           size);
    return;
  }

  // 2. Cast the raw bytes into our structured "Thought"
  AISignal *incoming_thought = (AISignal *)data;

  printf("\n[+] --- INCOMING THOUGHT ---\n");
  printf("[+] From Socket: %d\n", client_fd);
  printf("[+] Message ID: %u\n", incoming_thought->message_id);
  printf("[+] Energy Left: %d\n", incoming_thought->energy);

  // 3. Routing Logic
  if (incoming_thought->is_return == 1) {
    printf("[+] Type: Returning Boomerang (Consensus)\n");
    // TODO: Look up the breadcrumb cache and send it backward!

  } else if (incoming_thought->is_correction == 1) {
    printf("[+] Type: Correction Ripple (Learning)\n");
    // TODO: Pass to ai_process_correction() and route backward!

  } else {
    printf("[+] Type: Forward Ripple (Prediction)\n");

    // Save where this came from so we can return the answer later
    protocol_save_breadcrumb(incoming_thought->message_id, client_fd);

    if (incoming_thought->energy == 0) {
      printf("[*] Energy depleted. Turning Boomerang around...\n");
      incoming_thought->is_return = 1;
      // TODO: Send it back to where it came from
    } else {
      // Drop it into the brain!
      AISignal outgoing_thought;
      ai_process_forward_signal(incoming_thought, &outgoing_thought);

      // TODO: Broadcast 'outgoing_thought' to all other connected peers
      printf("[*] Brain generated response. Ready to broadcast.\n");
    }
  }
  printf("[+] ------------------------\n\n");
}

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, handle_shutdown_signal);
  signal(SIGTERM, handle_shutdown_signal);

  if (argc < 2) {
    printf("Usage: %s <path_to_config.json>\n", argv[0]);
    return 1;
  }

  NodeConfig config;
  if (load_config(argv[1], &config) != 0) {
    return 1;
  }

  strncpy(my_node_id, config.node_id, sizeof(my_node_id));

  // --- NEW: INITIALIZE THE AI AND ROUTING ---
  protocol_init_cache();
  ai_init_brain();

  ai_node_set_callback(incoming_data_handler);

  printf("[App] Booting Engine on port %d (Node ID: %s)...\n", config.port,
         my_node_id);

  ai_node_start(config.port, config.target_ip, config.target_port,
                config.num_threads, config.max_queue_size);

  return 0;
}
