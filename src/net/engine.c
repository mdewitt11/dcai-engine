#include "engine.h"
#include "protocol.h"
#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EVENTS 64
SwarmPeer connected_peers[100];
int peer_count = 0;

// Change the name so the compiler CANNOT confuse it with anything else
void add_peer_to_swarm_V2(int fd, int listen_port) {
  if (peer_count < 100) {
    connected_peers[peer_count].fd = fd; // Note the .fd!
    connected_peers[peer_count].listen_port =
        listen_port; // Note the .listen_port!
    peer_count++;
    printf("[Swarm] Added fd %d (Port %d) to Address Book! (Total: %d)\n", fd,
           listen_port, peer_count);
  } else {
    printf("[-] Swarm is full! Cannot add peer on fd %d\n", fd);
  }
}

// The Mouth
void ai_node_broadcast(AISignal *signal) {
  printf("[Net] Broadcasting thought (MsgID: %u) to %d swarm peers...\n",
         signal->message_id, peer_count);
  for (int i = 0; i < peer_count; i++) {
    write(connected_peers[i].fd, signal, sizeof(AISignal));
  }
}
