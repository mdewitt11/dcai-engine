#ifndef PEER_H
#define PEER_H

void handle_new_connection(int epoll_fd, int master_socket);
char *handle_client_data(int peer_fd, int *bytes_read);

#endif
