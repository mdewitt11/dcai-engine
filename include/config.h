#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
  int port;       // Renamed from my_port
  int admin_port; // NEW
  char admin_key[64];
  char node_id[128]; // Now a string instead of an int
  char target_ip[64];
  int target_port;
  int num_threads;
  int max_queue_size;
} NodeConfig;

int load_config(const char *filepath, NodeConfig *config);

#endif
