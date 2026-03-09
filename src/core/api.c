#include "api.h"
#include "client.h"
#include "engine.h"
#include "server.h"
#include "threadpool.h"
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

// Global pointer to store the user's callback function
on_receive_func app_callback = NULL;

void ai_node_set_callback(on_receive_func callback) {
  app_callback = callback;
  printf("[API] Application callback registered successfully.\n");
}

void ai_node_start(int my_port, const char *peer_ip, int peer_port,
                   int num_threads, int queue_limit) {
  int outbound_socket = -1;

  if (peer_ip != NULL && peer_port > 0) {
    outbound_socket = connect_to_node(peer_ip, peer_port);
  }

  ThreadPool *pool = threadpool_create(num_threads, queue_limit);
  int master_socket = setup_listening_socket(my_port);

  // This will now block UNTIL keep_running is set to 0
  run_epoll_engine(master_socket, outbound_socket, my_port, pool);

  // WHEN IT UNBLOCKS, CLEAN UP!
  threadpool_destroy(pool);
  printf("[*] Node successfully shut down. Goodbye!\n");
}

int ai_node_send(int target_fd, char *data, int size) {
  // A simple wrapper around the raw socket send.
  // In a production system, you'd wrap this in a while loop to handle EAGAIN
  int sent = send(target_fd, data, size, 0);
  if (sent < 0) {
    perror("[API] Send failed");
  }
  return sent;
}
