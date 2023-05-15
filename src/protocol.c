#include <fcntl.h>

#include "includeme.h"

/*
 * Send a packet, which consists of a fixed-size header followed by an
 * optional associated data payload.
 *
 * @param fd  The file descriptor on which packet is to be sent.
 * @param hdr  The fixed-size packet header, with multi-byte fields
 *   in network byte order
 * @param data  The data payload, or NULL, if there is none.
 * @return  0 in case of successful transmission, -1 otherwise.
 *   In the latter case, errno is set to indicate the error.
 *
 * All multi-byte fields in the packet are assumed to be in network byte order.
 */
int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
  int bytes_sent = 0;
  int bytes_to_send = sizeof(JEUX_PACKET_HEADER);
  int bytes_left = bytes_to_send;
  int bytes_written = 0;

  info("WRITING PACKET: type=%d, size=%d, id=%d, role=%d", hdr->type, ntohs(hdr->size), hdr->id, hdr->role);

  while (bytes_sent < bytes_to_send) {
    bytes_written = write(fd, ((void *)hdr) + bytes_sent, bytes_left);
    if (bytes_written == 0) {
      debug("Reached EOF (bytes written header)");
      return -1;
    }
    if (bytes_written < 0) {
      error("error writing packet header");
      return -1;
    }
    bytes_sent += bytes_written;
    bytes_left -= bytes_written;
    // debug("written %d bytes out of %d", bytes_sent, bytes_to_send);
  }
  int bytes_size = ntohs(hdr->size);
  // warn("bytes_size: %d", bytes_size);
  // convert size back to host byte order

  bytes_sent = 0;
  bytes_to_send = bytes_size;
  bytes_left = bytes_to_send;

  if (bytes_left == 0 && data == NULL) {
    return 0;
  }

  if (bytes_left > 0 && data == NULL) {
    error("data is null but bytes_left > 0");
    return -1;
  }

  if (bytes_left == 0 && data != NULL) {
    error("data is not null but bytes_left == 0");
    return -1;
  }

  while (bytes_sent < bytes_to_send) {
    debug("written %d bytes out of %d", bytes_sent, bytes_to_send);
    bytes_written = write(fd, data + bytes_sent, bytes_left);
    if (bytes_written < 0) {
      error("error writing packet data");
      return -1;
    }
    if (bytes_written == 0) {
      debug("Reached EOF (bytes written payload)");
      return -1;
    }
    bytes_sent += bytes_written;
    bytes_left -= bytes_written;
  }
  return 0;
}

/*
 * Receive a packet, blocking until one is available.
 *
 * @param fd  The file descriptor from which the packet is to be received.
 * @param hdr  Pointer to caller-supplied storage for the fixed-size
 *   packet header.
 * @param datap  Pointer to a variable into which to store a pointer to any
 *   payload received.
 * @return  0 in case of successful reception, -1 otherwise.  In the
 *   latter case, errno is set to indicate the error.
 *
 * The returned packet has all multi-byte fields in network byte order.
 * If the returned payload pointer is non-NULL, then the caller has the
 * responsibility of freeing that storage.
 */
int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
  int bytes_read = 0;
  int bytes_to_read = sizeof(JEUX_PACKET_HEADER);
  int bytes_left = bytes_to_read;
  while (bytes_read < bytes_to_read) {
    int bytes_received = read(fd, ((void *)hdr) + bytes_read, bytes_left);
    if (bytes_received < 0) {
      error("nothing to read");
      return -1;
    }
    if (bytes_received == 0) {
      info("READING PACKET: type=%d, size=%d, id=%d, role=%d", hdr->type, ntohs(hdr->size), hdr->id, hdr->role);
      info("read eof");
      return -1;
    }
    bytes_read += bytes_received;
    bytes_left -= bytes_received;
    // warn("i read: %d", bytes_received);
  }
  // Convert multi-byte fields in header to host byte order
  info("READING PACKET: type=%d, size=%d, id=%d, role=%d", hdr->type, ntohs(hdr->size), hdr->id, hdr->role);

  int bytes_size = ntohs(hdr->size);
  debug("bytes_size: %d", bytes_size);

  if (bytes_size <= 0) {
    *payloadp = NULL;
    error("payload size is 0");
    return 0;
  } else if (payloadp == NULL) {
    error("payloadp is null but bytes_size > 0");
    return -1;
  }

  *payloadp = calloc(bytes_size + 1, sizeof(char));
  if (*payloadp == NULL) {
    error("cow licked incorrectly");
    return -1;
  }

  bytes_read = 0;
  bytes_to_read = bytes_size;
  bytes_left = bytes_to_read;
  while (bytes_read < bytes_to_read) {
    // debug("bytes read %d out of %d", bytes_read, bytes_to_read);
    int bytes_received = read(fd, *payloadp + bytes_read, bytes_left);
    // debug("payload read: %d", bytes_received);
    if (bytes_received < 0) {
      free(*payloadp);
      payloadp = NULL;
      error("error reading payload");
      return -1;
    } 
    if (bytes_received == 0) {
      info("EOF reached");
      return -1;
    }
    bytes_read += bytes_received;
    bytes_left -= bytes_received;
  }
  info("payload read: %s", (char *) *payloadp);
  return 0;
}
