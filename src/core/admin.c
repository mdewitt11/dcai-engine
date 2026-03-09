#include "admin.h"
#include "ai.h"
#include "protocol.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void process_admin_command(int client_fd, AdminCommand *cmd,
                           NodeConfig *config) {
  // 1. HARDEN THE INPUT: Force null-terminators to prevent buffer overflows
  cmd->auth_key[sizeof(cmd->auth_key) - 1] = '\0';
  cmd->human_data[sizeof(cmd->human_data) - 1] = '\0';

  // 2. VERIFY THE KEY: Secure comparison
  if (strncmp(cmd->auth_key, config->admin_key, sizeof(cmd->auth_key)) != 0) {
    printf("[!] UNAUTHORIZED ADMIN ACCESS ATTEMPT on socket %d. Dropping.\n",
           client_fd);
    return; // In a real engine, we'd forcibly close the socket here
  }

  printf("[Admin] Access Granted. Processing human input: %s\n",
         cmd->human_data);

  // 3. THE BRIDGE: Translate Text to AI Signal
  AISignal new_thought;
  new_thought.message_id = rand(); // Generate a unique ID for the swarm
  new_thought.is_return = 0;
  new_thought.is_correction = cmd->is_correction;
  new_thought.energy = 5; // The initial pebble drop energy!

  // Clear the origin node ID and set it to ourselves
  memset(new_thought.origin_node_id, 0, sizeof(new_thought.origin_node_id));
  strncpy(new_thought.origin_node_id, config->node_id,
          sizeof(new_thought.origin_node_id) - 1);

  // Hash the human string into the 16-float tensor
  for (int i = 0; i < TENSOR_SIZE; i++) {
    if ((size_t)i < strlen(cmd->human_data)) {
      new_thought.tensor_data[i] = (float)cmd->human_data[i] / 255.0f;
    } else {
      new_thought.tensor_data[i] = 0.0f;
    }
  }

  // 4. Drop it into the brain!
  AISignal network_response;
  ai_process_forward_signal(&new_thought, &network_response);

  printf("[Admin] Translation complete. Brain generated thought. Ready to "
         "broadcast to swarm.\n");
  // TODO: Actually send network_response out to the connected peers
}
