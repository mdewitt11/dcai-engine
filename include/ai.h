#ifndef CUSTOM_AI_H
#define CUSTOM_AI_H

#include "protocol.h"
#include <stdint.h>

#define MAX_MEMORY_CELLS 5000
#define MATCH_THRESHOLD                                                        \
  0.5f // How close a signal needs to be for a "Fuzzy Match"

// The physical structure of a single memory
typedef struct {
  float input_signal[TENSOR_SIZE];    // What we saw
  float output_response[TENSOR_SIZE]; // What we did
  uint32_t usage_count;               // How many times it worked
  float confidence;                   // 0.0 to 1.0
} MemoryCell;

// Global Brain Array
extern MemoryCell brain[MAX_MEMORY_CELLS];
extern int memory_count;

// Core AI Functions
void ai_init_brain();
void ai_process_forward_signal(AISignal *incoming_signal,
                               AISignal *outgoing_signal);
void ai_process_correction(AISignal *correction_ripple);

#endif
