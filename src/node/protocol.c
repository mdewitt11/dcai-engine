#include "protocol.h"
#include <pthread.h>
#include <stdio.h>

// The global breadcrumb cache for the node
Breadcrumb trail_cache[MAX_BREADCRUMBS];
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

// Returns 1 if we've seen this thought before, 0 if it is brand new
int protocol_is_duplicate(uint32_t msg_id) {
  pthread_mutex_lock(&cache_lock);
  int is_dup = 0;

  for (int i = 0; i < MAX_BREADCRUMBS; i++) {
    if (trail_cache[i].message_id == msg_id && trail_cache[i].source_fd != -1) {
      is_dup = 1; // Found it! It's an echo.
      break;
    }
  }

  pthread_mutex_unlock(&cache_lock);
  return is_dup;
}

void protocol_init_cache() {
  pthread_mutex_lock(&cache_lock);
  for (int i = 0; i < MAX_BREADCRUMBS; i++) {
    trail_cache[i].message_id = 0;
    trail_cache[i].source_fd = -1; // -1 means empty
    trail_cache[i].timestamp = 0;
  }
  pthread_mutex_unlock(&cache_lock);
  printf("[Protocol] Breadcrumb routing cache initialized.\n");
}

void protocol_save_breadcrumb(uint32_t msg_id, int source_fd) {
  pthread_mutex_lock(&cache_lock);

  time_t now = time(NULL);
  int slot_to_use = -1;
  time_t oldest_time = now + 1;
  int oldest_slot = 0;

  // Look for an empty slot or an expired one
  for (int i = 0; i < MAX_BREADCRUMBS; i++) {
    if (trail_cache[i].source_fd == -1 ||
        (now - trail_cache[i].timestamp > BREADCRUMB_TIMEOUT_SEC)) {
      slot_to_use = i;
      break;
    }
    // Track the oldest slot in case the cache is completely full
    if (trail_cache[i].timestamp < oldest_time) {
      oldest_time = trail_cache[i].timestamp;
      oldest_slot = i;
    }
  }

  // If cache is 100% full and nothing is expired, overwrite the oldest
  if (slot_to_use == -1) {
    slot_to_use = oldest_slot;
  }

  trail_cache[slot_to_use].message_id = msg_id;
  trail_cache[slot_to_use].source_fd = source_fd;
  trail_cache[slot_to_use].timestamp = now;

  pthread_mutex_unlock(&cache_lock);
}

int protocol_get_return_fd(uint32_t msg_id) {
  pthread_mutex_lock(&cache_lock);

  int return_fd = -1;
  for (int i = 0; i < MAX_BREADCRUMBS; i++) {
    if (trail_cache[i].message_id == msg_id && trail_cache[i].source_fd != -1) {
      return_fd = trail_cache[i].source_fd;
      // We found it, so we can clear this breadcrumb to save space
      trail_cache[i].source_fd = -1;
      break;
    }
  }

  pthread_mutex_unlock(&cache_lock);
  return return_fd;
}
