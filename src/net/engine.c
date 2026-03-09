#include "engine.h"
#include "peer.h"
#include "server.h"
#include "threadpool.h" // ADD THIS
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EVENTS 64

// ADD THE THREADPOOL ARGUMENT HERE
void run_epoll_engine(int master_socket, int outbound_socket, int port,
                      ThreadPool *pool) {
  struct epoll_event event, events[MAX_EVENTS];
  int epoll_fd = epoll_create1(0);

  if (epoll_fd == -1) {
    perror("epoll_create1 failed");
    exit(EXIT_FAILURE);
  }

  // 1. Register the Master Socket (Listening for incoming peers)
  event.events = EPOLLIN;
  event.data.fd = master_socket;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, master_socket, &event);

  // 2. NEW: Register the Outbound Socket (If we connected to someone)
  if (outbound_socket > 0) {
    set_nonblocking(outbound_socket);
    struct epoll_event out_event;
    out_event.events = EPOLLIN | EPOLLET; // Edge-triggered
    out_event.data.fd = outbound_socket;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, outbound_socket, &out_event);
    printf("[*] Engine is now monitoring outbound connection on socket %d\n",
           outbound_socket);
  }

  printf("[*] AI Node awake. Engine running on port %d\n", port);

  while (keep_running) {
    int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 500);

    if (event_count == -1) {
      // EINTR means "Interrupted by System Call" (like Ctrl+C). This is normal!
      if (errno == EINTR)
        continue;
      perror("epoll_wait failed");
      break;
    }
    for (int i = 0; i < event_count; i++) {
      if (events[i].data.fd == master_socket) {
        handle_new_connection(epoll_fd, master_socket);
      } else {
        int bytes_read = 0;
        int peer_fd = events[i].data.fd;
        char *data_buffer = handle_client_data(peer_fd, &bytes_read);

        if (data_buffer != NULL && bytes_read > 0) {
          // INSTANTLY THROW THE WORK TO THE THREAD POOL
          threadpool_submit(pool, peer_fd, data_buffer, bytes_read);

          // Notice we DO NOT free(data_buffer) here anymore!
          // The worker thread will free it when it finishes the math.
        }
      }
    }
  }

  printf("\n[*] Engine loop terminated. Closing network sockets...\n");
  close(master_socket);
  if (outbound_socket > 0)
    close(outbound_socket);
  close(epoll_fd);
  close(master_socket);
  close(epoll_fd);
}
