#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   // Needed for random seed
#include <unistd.h> // Needed for gethostname()

static void generate_unique_node_id(char *buffer, size_t max_len) {
  char hostname[64] = "unknown_host";
  gethostname(hostname, sizeof(hostname));

  // Seed the random number generator using time and the process ID
  srand(time(NULL) ^ getpid());

  // Combine hostname with a random 8-character hex string
  snprintf(buffer, max_len, "%s-%04X%04X", hostname, rand() % 0xFFFF,
           rand() % 0xFFFF);
}

// Ultra-forgiving integer parser
int get_json_int(const char *json, const char *key, int default_val) {
  char *pos =
      strstr(json, key); // Look for the key (even if quotes are missing/weird)
  if (pos) {
    pos = strchr(pos, ':'); // Find the colon after the key
    if (pos) {
      int val;
      if (sscanf(pos + 1, " %d", &val) == 1) {
        return val; // Successfully parsed!
      }
    }
  }
  printf("[!] Warning: Could not parse '%s'. Defaulting to %d\n", key,
         default_val);
  return default_val;
}

// Ultra-forgiving string parser
void get_json_string(const char *json, const char *key, char *out_buffer,
                     const char *default_val) {
  strcpy(out_buffer, default_val); // Set default first

  const char *pos = strstr(json, key);
  if (pos) {
    pos = strchr(pos, ':');
    if (pos) {
      char *start_quote = strchr(pos, '"');
      if (start_quote) {
        start_quote++; // Move past the quote
        char *end_quote = strchr(start_quote, '"');
        if (end_quote) {
          int len = end_quote - start_quote;
          strncpy(out_buffer, start_quote, len);
          out_buffer[len] = '\0'; // Null terminate
          return;
        }
      }
    }
  }
  printf("[!] Warning: Could not parse string '%s'. Defaulting to '%s'\n", key,
         default_val);
}

int load_config(const char *filepath, NodeConfig *config) {
  FILE *file = fopen(filepath, "r");
  if (!file) {
    printf("[!] Could not open config file: %s\n", filepath);
    return -1;
  }

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (length <= 0) {
    printf("[!] Error: Config file '%s' is empty!\n", filepath);
    fclose(file);
    return -1;
  }

  char *json_buffer = malloc(length + 1);
  if (json_buffer) {
    fread(json_buffer, 1, length, file);
    json_buffer[length] = '\0';
  } else {
    fclose(file);
    return -1;
  }
  fclose(file);

  // --- DEBUG PRINT: Let's see exactly what C is reading ---
  printf("--- READING RAW FILE ---\n%s\n------------------------\n",
         json_buffer);

  config->port = get_json_int(json_buffer, "port", 8000);
  generate_unique_node_id(config->node_id, sizeof(config->node_id));
  get_json_string(json_buffer, "target_ip", config->target_ip, "");
  config->admin_port = get_json_int(json_buffer, "admin_port", 8080);
  get_json_string(json_buffer, "admin_key", config->admin_key,
                  "default_secret_key");
  config->target_port = get_json_int(json_buffer, "target_port", 0);
  config->num_threads = get_json_int(json_buffer, "num_threads", 4);
  config->max_queue_size = get_json_int(json_buffer, "max_queue_size", 10000);

  free(json_buffer);
  printf("[*] Loaded config from %s\n", filepath);
  return 0;
}
