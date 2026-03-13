#ifndef ENGINE_H
#define ENGINE_H

#include "protocol.h"
#include "threadpool.h"
#include <signal.h>

#define MAX_PEERS 100

// The "extern" keyword tells C: "These exist somewhere else in the code, just
// trust me and share them."
// --- THE NEW ADDRESS BOOK ---
typedef struct {
  int fd;          // The local socket to talk to them
  char ip[16];     // Their IP address (e.g., "127.0.0.1")
  int listen_port; // The port THEY are listening on for new friends
} SwarmPeer;

// Replace 'int connected_peers[100];' with this:
extern SwarmPeer connected_peers[100];
extern int peer_count;

// The 'extern' keyword tells the compiler: "This exists, trust me."
extern volatile sig_atomic_t keep_running;

void run_epoll_engine(int master_socket, int outbound_socket, int port,
                      ThreadPool *pool);
void add_peer_to_swarm_V2(int client_fd, int listen_port);
void ai_node_broadcast(AISignal *signal);

#endif
