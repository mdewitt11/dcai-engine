#ifndef ENGINE_H
#define ENGINE_H

#include "protocol.h"
#include "threadpool.h"
#include <signal.h>

// The 'extern' keyword tells the compiler: "This exists, trust me."
extern volatile sig_atomic_t keep_running;

void run_epoll_engine(int master_socket, int outbound_socket, int port,
                      ThreadPool *pool);
void add_peer_to_swarm(int client_fd);
void ai_node_broadcast(AISignal *signal);

#endif
