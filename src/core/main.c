#include "libnetwork.h"
#include <stdio.h>

// We must declare the Go exported function so C knows it exists!
extern void GoRouteAnswer(char *msg_id, char *answer);

// Updated signature to match Go
void c_receive_thought(char *msg_id, char *msg) {
  printf("[C Brain] Processing Thought (%s): '%s'\n", msg_id, msg);

  // 2. Query your Neural Graph (Simulated here)
  char output_buffer[512];
  snprintf(output_buffer, sizeof(output_buffer),
           "I am the AI. My calculated response to '%s' is 42.", msg);

  // 3. FIRE IT BACK TO GO!
  GoRouteAnswer(msg_id, output_buffer);
}

int main(int argc, char **argv) {
  char *config_file = "config.json";
  if (argc > 1) {
    config_file = argv[1];
  }

  printf("========================================\n");
  printf(" 🧠 DCAI POLYGLOT ENGINE ONLINE \n");
  printf("========================================\n");

  StartSwarmNetwork(config_file);
  return 0;
}
