#ifndef SERVER_H
#define SERVER_H

#include "config.h"
typedef enum {
  SOCKET_SWARM_LISTENER,
  SOCKET_ADMIN_LISTENER,
  SOCKET_SWARM_PEER,
  SOCKET_ADMIN_CLIENT
} SocketType;

// This wrapper tells epoll exactly what is waking it up
typedef struct {
  int fd;
  SocketType type;
} SocketContext;

int set_nonblocking(int fd);
int setup_listening_socket(int port);
void engine_start_multiplexed(NodeConfig *config, int outbound_sockect);

#endif
