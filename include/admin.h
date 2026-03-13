#ifndef ADMIN_H
#define ADMIN_H

#include "config.h"
#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
  char auth_key[64]; // The admin key to prove you own the node
  uint8_t
      is_correction; // 0 = "Here is a question", 1 = "Here is the right answer"
  uint8_t use_encryption; // 0 = Plaintext, 1 = Encrypted payload

  // The actual human data (e.g., text from your script)
  // We keep it a fixed size so hackers can't buffer-overflow the node
  char human_data[256];
} AdminCommand;
#pragma pack(pop)

void process_admin_command(const char *json_payload, int client_fd,
                           NodeConfig *config);

#endif
