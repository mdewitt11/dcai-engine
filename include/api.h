#ifndef API_H
#define API_H

// 1. Define a "Callback" type.
// This is a function signature that the user's application will provide to us.
typedef void (*on_receive_func)(int client_fd, char *data, int size);

// 2. The Public API Functions
void ai_node_start(int my_port, const char *peer_ip, int peer_port,
                   int num_threads, int queue_limit);
void ai_node_set_callback(on_receive_func callback);
int ai_node_send(int target_fd, char *data, int size);

#endif
