#include "admin.h"
#include "config.h" // For get_json_string and get_json_int
#include "engine.h"
#include "protocol.h" // For AISignal and Breadcrumbs
#include "transformer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void process_admin_command(const char *json_payload, int client_fd,
                           NodeConfig *config) {
  printf("\n[Admin] Received payload: %s\n", json_payload);

  // ==========================================
  // 1. CRASH-PROOF PASSWORD EXTRACTION
  // ==========================================
  printf("[Debug] 1. Extracting Password...\n");
  char provided_pass[64] = {0};
  char *pass_ptr = strstr(json_payload, "\"pass\"");
  if (pass_ptr)
    pass_ptr = strchr(pass_ptr, ':');
  if (pass_ptr)
    pass_ptr = strchr(pass_ptr, '"');
  if (pass_ptr) {
    pass_ptr++; // Move past the quote
    char *end_quote = strchr(pass_ptr, '"');
    if (end_quote && (end_quote - pass_ptr) < 63) {
      strncpy(provided_pass, pass_ptr, end_quote - pass_ptr);
    }
  }

  printf("[Debug] 2. Checking Password: '%s'\n", provided_pass);
  if (strcmp(provided_pass, config->admin_key) != 0) {
    printf("[-] Admin Auth Failed! Incorrect password.\n");
    char *err = "ERROR: Unauthorized\n";
    write(client_fd, err, strlen(err));
    close(client_fd);
    return;
  }

  // ==========================================
  // 3. CRASH-PROOF TEXT EXTRACTION
  // ==========================================
  printf("[Debug] 3. Extracting Text...\n");
  char incoming_text[512] = {0};
  char *text_ptr = strstr(json_payload, "\"text\"");
  if (text_ptr)
    text_ptr = strchr(text_ptr, ':');
  if (text_ptr)
    text_ptr = strchr(text_ptr, '"');
  if (text_ptr) {
    text_ptr++; // Move past the quote
    char *end_quote = strchr(text_ptr, '"');
    if (end_quote && (end_quote - text_ptr) < 511) {
      strncpy(incoming_text, text_ptr, end_quote - text_ptr);
    }
  }

  // ==========================================
  // 4. CRASH-PROOF ENERGY EXTRACTION
  // ==========================================
  printf("[Debug] 4. Extracting Energy...\n");
  int requested_energy = 5; // Default
  char *energy_ptr = strstr(json_payload, "\"energy\"");
  if (energy_ptr)
    energy_ptr = strchr(energy_ptr, ':');
  if (energy_ptr) {
    energy_ptr++; // Move past the colon
    requested_energy =
        atoi(energy_ptr); // Convert the number text to an integer
  }

  if (requested_energy <= 0)
    requested_energy = 5;
  if (requested_energy > MAX_SWARM_ENERGY)
    requested_energy = MAX_SWARM_ENERGY;

  // ==========================================
  // 5. BUILD AND BROADCAST
  // ==========================================
  printf("[Debug] 5. Zeroing out Struct memory...\n");
  AISignal genesis_thought;
  memset(&genesis_thought, 0, sizeof(AISignal));

  genesis_thought.packet_type = PACKET_TYPE_THOUGHT;
  genesis_thought.message_id = (uint32_t)rand();
  genesis_thought.energy = requested_energy;

  printf("[Debug] 6. Running Text-to-Tensor translation...\n");
  int tensor_size = sizeof(genesis_thought.tensor_data) /
                    sizeof(genesis_thought.tensor_data[0]);
  text_to_tensor(incoming_text, genesis_thought.tensor_data, tensor_size);

  printf("[Debug] 7. Struct built! Broadcasting to Swarm...\n");
  protocol_save_breadcrumb(genesis_thought.message_id, -1);
  ai_node_broadcast(&genesis_thought);

  char *success = "SUCCESS: Thought Injected into Swarm\n";
  write(client_fd, success, strlen(success));
  close(client_fd);
}
