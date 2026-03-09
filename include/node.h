#ifndef NODE_H
#define NODE_H

#define MAX_PEERS 30
#define BUFFER_SIZE 1024

// Function prototypes
void set_nonblocking(int sock);
int setup_listening_socket(int port);
void run_event_loop(int master_socket, int port);

#endif // NODE_H
