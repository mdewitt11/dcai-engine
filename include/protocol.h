#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <time.h>

#define TENSOR_SIZE 128
#define MAX_BREADCRUMBS 1000
#define BREADCRUMB_TIMEOUT_SEC 60 // Forget paths older than 60 seconds
// The absolute maximum network hops a thought can survive
#define MAX_SWARM_ENERGY 10

// --- THE NETWORK PAYLOAD ---
// Pack(1) ensures no padding bytes are added by the compiler
// Inside include/protocol.h
// We need to tell the parser what kind of packet is arriving!
#define PACKET_TYPE_HANDSHAKE 1
#define PACKET_TYPE_THOUGHT 2
#define PACKET_TYPE_PEER_LIST 3 // THE NEW PACKET

// 3. The Address Book Delivery Packet
typedef struct {
  uint8_t packet_type; // Always PACKET_TYPE_PEER_LIST (3)
  int num_peers;       // How many ports are in this list?
  int peer_ports[10];  // A quick array of up to 10 ports
} PeerListPacket;

// The Introduction Packet
typedef struct {
  uint8_t packet_type; // Always set to PACKET_TYPE_HANDSHAKE (1)
  int listen_port;     // "Hi, I am listening on port 8002!"
} SwarmHandshake;

// Let's also add the packet type to your existing AISignal!
// Add 'uint8_t packet_type;' to the very top of your AISignal struct.

#pragma pack(push, 1)
typedef struct {
  uint8_t packet_type;
  uint32_t message_id;
  char origin_node_id[64]; // <-- ADD THIS LINE
  uint8_t is_return;
  uint8_t is_correction;
  uint8_t energy;

  float tensor_data[TENSOR_SIZE];
} AISignal;
#pragma pack(pop)

// --- THE ROUTING CACHE ---
typedef struct {
  uint32_t message_id;
  int source_fd;    // The socket this message came from
  time_t timestamp; // When we saw it
} Breadcrumb;

// Protocol Functions
int protocol_is_duplicate(uint32_t msg_id);
void protocol_init_cache();
void protocol_save_breadcrumb(uint32_t msg_id, int source_fd);
int protocol_get_return_fd(uint32_t msg_id);

#endif
