#include "client.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int connect_to_node(const char *target_ip, int target_port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Socket creation failed");
    return -1;
  }

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(target_port);

  // Convert IPv4 address from text to binary form
  if (inet_pton(AF_INET, target_ip, &serv_addr.sin_addr) <= 0) {
    printf("[!] Invalid address or address not supported: %s\n", target_ip);
    close(sock);
    return -1;
  }

  printf("[*] Attempting to connect to %s:%d...\n", target_ip, target_port);

  // Attempt to establish the TCP connection
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("[!] Connection to %s:%d failed. Are they online?\n", target_ip,
           target_port);
    close(sock);
    return -1;
  }

  printf("[+] Successfully connected to Node at %s:%d!\n", target_ip,
         target_port);
  return sock; // Return the connected socket file descriptor
}
