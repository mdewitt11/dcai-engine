#include "threadpool.h"
#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern on_receive_func app_callback;

void *thread_worker(void *arg) {
  ThreadPool *pool = (ThreadPool *)arg;

  while (1) {
    pthread_mutex_lock(&(pool->lock));

    while (pool->queue_head == NULL && !pool->stop) {
      pthread_cond_wait(&(pool->notify), &(pool->lock));
    }

    if (pool->stop && pool->queue_head == NULL) {
      pthread_mutex_unlock(&(pool->lock));
      break;
    }

    // --- GRAB THE TASK AND REDUCE THE QUEUE SIZE ---
    Task *task = pool->queue_head;
    pool->queue_head = task->next;
    if (pool->queue_head == NULL) {
      pool->queue_tail = NULL;
    }
    pool->current_queue_size--; // We freed up a slot!

    // Tell the network engine (if it was blocked) that there is space now
    pthread_cond_signal(&(pool->queue_not_full));

    pthread_mutex_unlock(&(pool->lock));

    // Hand data to the application layer
    if (app_callback != NULL) {
      app_callback(task->client_fd, task->data, task->data_size);
    }

    free(task->data);
    free(task);
  }
  return NULL;
}

ThreadPool *threadpool_create(int num_threads, int max_queue_size) {
  ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));
  pool->thread_count = num_threads;
  pool->max_queue_size = max_queue_size;
  pool->current_queue_size = 0;
  pool->stop = 0;
  pool->queue_head = NULL;
  pool->queue_tail = NULL;

  pthread_mutex_init(&(pool->lock), NULL);
  pthread_cond_init(&(pool->notify), NULL);
  pthread_cond_init(&(pool->queue_not_full), NULL); // Init the new condition

  pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);

  for (int i = 0; i < num_threads; i++) {
    pthread_create(&(pool->threads[i]), NULL, thread_worker, (void *)pool);
  }

  printf("[*] Thread Pool created (%d workers, Max Queue: %d).\n", num_threads,
         max_queue_size);
  return pool;
}

void threadpool_submit(ThreadPool *pool, int client_fd, char *data, int size) {
  pthread_mutex_lock(&(pool->lock));

  // --- BACKPRESSURE WAITING LOOP ---
  // If the queue is full, the network engine goes to sleep right here!
  while (pool->current_queue_size >= pool->max_queue_size && !pool->stop) {
    printf("[!] WARNING: Queue full (%d/%d). Throttling network...\n",
           pool->current_queue_size, pool->max_queue_size);
    pthread_cond_wait(&(pool->queue_not_full), &(pool->lock));
  }

  if (pool->stop) {
    pthread_mutex_unlock(&(pool->lock));
    free(data); // Don't leak memory if we are shutting down
    return;
  }

  Task *new_task = (Task *)malloc(sizeof(Task));
  new_task->client_fd = client_fd;
  new_task->data = data;
  new_task->data_size = size;
  new_task->next = NULL;

  if (pool->queue_tail == NULL) {
    pool->queue_head = new_task;
    pool->queue_tail = new_task;
  } else {
    pool->queue_tail->next = new_task;
    pool->queue_tail = new_task;
  }

  pool->current_queue_size++; // We filled a slot

  pthread_cond_signal(&(pool->notify)); // Wake up a worker
  pthread_mutex_unlock(&(pool->lock));
}

void threadpool_destroy(ThreadPool *pool) {
  if (pool == NULL)
    return;

  pthread_mutex_lock(&(pool->lock));
  pool->stop = 1;
  pthread_cond_broadcast(&(pool->notify));
  pthread_cond_broadcast(
      &(pool->queue_not_full)); // Unblock the network engine if it was stuck
  pthread_mutex_unlock(&(pool->lock));

  // ... (The rest of the destroy function remains the same: joins, freeing
  // queue, etc.) ...
}
