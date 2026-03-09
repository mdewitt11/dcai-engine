#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

typedef struct Task {
  int client_fd;
  char *data;
  int data_size;
  struct Task *next;
} Task;

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t notify; // Wakes up workers when tasks arrive
  pthread_cond_t
      queue_not_full; // NEW: Wakes up the network engine when space clears up

  pthread_t *threads;
  Task *queue_head;
  Task *queue_tail;

  int thread_count;
  int current_queue_size; // NEW: How many tasks are waiting?
  int max_queue_size;     // NEW: The hard limit
  int stop;
} ThreadPool;

// Update the creation signature
ThreadPool *threadpool_create(int num_threads, int max_queue_size);
void threadpool_submit(ThreadPool *pool, int client_fd, char *data, int size);
void threadpool_destroy(ThreadPool *pool);

#endif
