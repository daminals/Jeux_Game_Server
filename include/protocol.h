/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * The "Jeux" game server protocol.
 *
 * This header file specifies the format of communication between the
 * Jeux server and its clients.  We will use the term "packet" to
 * refer to a single message sent at the protocol level.  A
 * full-duplex, stream-based (i.e. TCP) connection is used between a
 * client and the server.  Communication is effected by the client and
 * server sending "packets" to each other over this connection.  Each
 * packet consists of a fixed-length header, with fields in network
 * byte order, followed by an optional payload whose length is
 * specified in the header.
 *
 * The following are the packet types in the protocol.  For each type,
 * the purpose of that type of packet is described, and the parameters
 * sent in that type of packet are listed.  Fixed-size parameters are
 * generally sent in header fields reserved for such parameters.
 * Variable-size parameters are sent as payload.
 *
 * Client-to-server requests:
 *   LOGIN:    Log a username into the server
 *             Payload: username
 *   USERS:    Request a list of currently logged-in users
 *   INVITE:   Invite another user to a game
 *             Header: role to which the target is invited.
 *             Payload: username of target
 *   REVOKE:   Revoke an invitation previously made
 *             Header: invitation ID assigned by the source
 *   ACCEPT:   Accept an invitation made by another player
 *             Header: invitation ID assigned by the target
 *   DECLINE:  Decline an invitation made by another player
 *             Header: invitation ID assigned by the target
 *   MOVE:     Make a move in an ongoing game
 *             Header: invitation ID assigned by player making the move
 *             Payload: string that describes the move
 *   RESIGN:   Resign an ongoing game
 *             Header: invitation ID assigned by player who is resigning
 *
 * Server-to-client responses (synchronous):
 *   ACK:      Sent in response to a successful request
 *             Header (depends on request):
 *			invitation ID (for INVITE request)
 *             Payload (depends on request):
 *                      list of usernames (for USERS request)
 *                      initial game state
 *                        (for ACCEPT request sent by first player)
 *   NACK:     Sent in response to a failed request
 *
 * Server-to-client notifications (asynchronous):
 * Specific to client:
 *   INVITED   Sent when an invitation has been made
 *             Header: invitation ID
 *             Payload: user name of source
 *   REVOKED   Sent when an invitation has been revoked by source
 *             Header: invitation ID assigned by target
 *   ACCEPTED  Sent when an invitation has been accepted by target
 *             Header: invitation ID assigned by source
 *             Payload: string showing initial game state
 *   DECLINED  Sent when an invitation has been declined by target
 *             Header: invitation ID assigned by source
 *   MOVED     Sent when the opponent has made a move
 *             Header: invitation ID
 *             Payload: string showing game state after the move
 *   RESIGNED  Sent when the opponent has resigned
 *             Header: invitation ID assigned by recipient
 *   ENDED     Sent when a game has ended
 *             Header: invitation ID assigned by recipient
 *                     GAME_ROLE (none, first, second) of winner
 */

/*
 * Packet types.
 */
typedef enum {
    /* Unused */
    JEUX_NO_PKT,
    /* Client-to-server*/
    JEUX_LOGIN_PKT,
    JEUX_USERS_PKT,
    JEUX_INVITE_PKT,
    JEUX_REVOKE_PKT,
    JEUX_ACCEPT_PKT,
    JEUX_DECLINE_PKT,
    JEUX_MOVE_PKT,
    JEUX_RESIGN_PKT,
    /* Server-to-client responses (synchronous) */
    JEUX_ACK_PKT,
    JEUX_NACK_PKT,
    /* Server-to_client notifications (asynchronous) */
    JEUX_INVITED_PKT,
    JEUX_REVOKED_PKT,
    JEUX_ACCEPTED_PKT,
    JEUX_DECLINED_PKT,
    JEUX_MOVED_PKT,
    JEUX_RESIGNED_PKT,
    JEUX_ENDED_PKT
} JEUX_PACKET_TYPE;

/*
 * Type definitions for fields in packets.
 */
typedef uint32_t gameid_T;

/*
 * Fixed-size packet header (same for all packets).
 *
 * NOTE: all multibyte fields in all packets are assumed to be in
 * network byte order.
 */
typedef struct jeux_packet_header {
    uint8_t type;		   // Type of the packet
    uint8_t id;			   // Invitation ID
    uint8_t role;		   // Role of player in game
    uint16_t size;                 // Payload size (zero if no payload)
    uint32_t timestamp_sec;        // Seconds field of time packet was sent
    uint32_t timestamp_nsec;       // Nanoseconds field of time packet was sent
} JEUX_PACKET_HEADER;

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
int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data);

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
int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp);

#endif
