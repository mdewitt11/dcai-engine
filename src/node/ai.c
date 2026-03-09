#include "ai.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MemoryCell brain[MAX_MEMORY_CELLS];
int memory_count = 0;
pthread_mutex_t brain_lock = PTHREAD_MUTEX_INITIALIZER;

void ai_init_brain() {
  pthread_mutex_lock(&brain_lock);
  memory_count = 0;
  memset(brain, 0, sizeof(brain));
  pthread_mutex_unlock(&brain_lock);
  printf("[AI] Associative Memory initialized. Capacity: %d cells.\n",
         MAX_MEMORY_CELLS);
}

// Calculates how similar two arrays of numbers are. 0.0 means identical.
static float calculate_distance(float *a, float *b) {
  float sum = 0.0f;
  for (int i = 0; i < TENSOR_SIZE; i++) {
    float diff = a[i] - b[i];
    sum += (diff * diff);
  }
  return sqrt(sum); // Requires -lm in Makefile
}

// Generates a totally random response when the node is confused
static void generate_random_response(float *output) {
  for (int i = 0; i < TENSOR_SIZE; i++) {
    output[i] = ((float)rand() / (float)(RAND_MAX)) * 2.0f - 1.0f;
  }
}

void ai_process_forward_signal(AISignal *incoming_signal,
                               AISignal *outgoing_signal) {
  pthread_mutex_lock(&brain_lock);

  // Copy the routing info over to the outgoing signal
  memcpy(outgoing_signal, incoming_signal, sizeof(AISignal));
  outgoing_signal->energy--; // Degrade the energy!

  int best_match_index = -1;
  float best_distance = 999999.0f;

  // Search the memory bank for the closest match
  for (int i = 0; i < memory_count; i++) {
    float dist =
        calculate_distance(incoming_signal->tensor_data, brain[i].input_signal);
    if (dist < best_distance) {
      best_distance = dist;
      best_match_index = i;
    }
  }

  if (best_match_index != -1 && best_distance == 0.0f) {
    printf("[AI] Exact memory match found! (Cell %d)\n", best_match_index);
    memcpy(outgoing_signal->tensor_data,
           brain[best_match_index].output_response,
           sizeof(float) * TENSOR_SIZE);
    brain[best_match_index].usage_count++;

  } else if (best_match_index != -1 && best_distance < MATCH_THRESHOLD) {
    printf("[AI] Fuzzy match found. Distance: %f\n", best_distance);
    for (int i = 0; i < TENSOR_SIZE; i++) {
      outgoing_signal->tensor_data[i] =
          brain[best_match_index].output_response[i] * 0.99f;
    }
    brain[best_match_index].usage_count++;

  } else {
    printf("[AI] Signal unknown. Generating new thought...\n");
    generate_random_response(outgoing_signal->tensor_data);

    if (memory_count < MAX_MEMORY_CELLS) {
      // Save this new thought to the brain!
      memcpy(brain[memory_count].input_signal, incoming_signal->tensor_data,
             sizeof(float) * TENSOR_SIZE);
      memcpy(brain[memory_count].output_response, outgoing_signal->tensor_data,
             sizeof(float) * TENSOR_SIZE);
      brain[memory_count].usage_count = 1;
      brain[memory_count].confidence = 0.5f;
      memory_count++;
    }
  }

  pthread_mutex_unlock(&brain_lock);
}
