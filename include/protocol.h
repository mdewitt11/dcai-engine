#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <time.h>

#define TENSOR_SIZE 16
#define MAX_BREADCRUMBS 1000
#define BREADCRUMB_TIMEOUT_SEC 60 // Forget paths older than 60 seconds

// --- THE NETWORK PAYLOAD ---
// Pack(1) ensures no padding bytes are added by the compiler
// Inside include/protocol.h

#pragma pack(push, 1)
typedef struct {
  uint32_t message_id;
  char origin_node_id[64]; // <-- ADD THIS LINE
  uint8_t is_return;
  uint8_t is_correction;
  uint8_t energy;

  float tensor_data[TENSOR_SIZE];
} AISignal;
#pragma pack(pop)

// --- THE ROUTING CACHE ---
typedef struct {
  uint32_t message_id;
  int source_fd;    // The socket this message came from
  time_t timestamp; // When we saw it
} Breadcrumb;

// Protocol Functions
void protocol_init_cache();
void protocol_save_breadcrumb(uint32_t msg_id, int source_fd);
int protocol_get_return_fd(uint32_t msg_id);

#endif
