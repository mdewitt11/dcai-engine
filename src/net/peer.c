#include "peer.h"
#include "server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

void handle_new_connection(int epoll_fd, int master_socket) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int new_socket =
      accept(master_socket, (struct sockaddr *)&client_addr, &client_len);

  if (new_socket == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK)
      perror("accept failed");
    return;
  }

  printf("[+] New peer connected from %s:%d\n", inet_ntoa(client_addr.sin_addr),
         ntohs(client_addr.sin_port));

  set_nonblocking(new_socket);

  struct epoll_event event;
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = new_socket;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event);
}

// Inside peer.c
char *handle_client_data(int peer_fd, int *total_bytes_read) {
  int current_buffer_size = 4096;
  char *full_buffer = malloc(current_buffer_size);
  *total_bytes_read = 0;

  while (1) {
    // Read into the offset of the buffer we've already filled
    int valread = read(peer_fd, full_buffer + *total_bytes_read,
                       current_buffer_size - *total_bytes_read);

    if (valread == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // SUCCESS! We have drained the entire socket buffer.
        break;
      } else {
        perror("Read error");
        free(full_buffer);
        return NULL;
      }
    } else if (valread == 0) {
      printf("[-] Peer on socket %d dropped offline.\n", peer_fd);
      close(peer_fd);
      free(full_buffer);
      *total_bytes_read = 0;
      return NULL;
    } else {
      *total_bytes_read += valread;

      // If our buffer is getting full, double its size dynamically using
      // realloc!
      if (*total_bytes_read >= current_buffer_size) {
        current_buffer_size *= 2;
        full_buffer = realloc(full_buffer, current_buffer_size);
      }
    }
  }

  return full_buffer; // Return the fully assembled data packet
}
