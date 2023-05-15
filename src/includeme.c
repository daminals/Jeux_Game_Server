#include "includeme.h"

/*
 Function to create a brand new header for a packet
 Caller is responsible for freeing the header
 */
JEUX_PACKET_HEADER *create_header(int type, int id, int role, int size) {
  JEUX_PACKET_HEADER *hdr_send = calloc(1, sizeof(JEUX_PACKET_HEADER));
  // create a header
  // -------------------------------//
  // uint8_t type;		              // Type of the packet
  // uint8_t id;		            	  // Invitation ID
  // uint8_t role;                  // Role of player in game
  // uint16_t size;                 // Payload size (zero if no payload)
  // uint32_t timestamp_sec;        // Seconds field of time packet was
  // sent uint32_t timestamp_nsec;  // Nanoseconds field of time
  // ------------------------------ //
  hdr_send->type = type;
  hdr_send->id = id;
  hdr_send->role = role;
  hdr_send->size = htons(size);
  // get time and put it in header
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  hdr_send->timestamp_nsec = htonl(ts.tv_nsec);
  hdr_send->timestamp_sec = htonl(ts.tv_sec);
  // send packet
  return hdr_send;
}