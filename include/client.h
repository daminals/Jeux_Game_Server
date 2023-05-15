/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include "player.h"
#include "client_registry.h"

/*
 * A CLIENT represents the state of a network client connected to the
 * system.  It contains the file descriptor of the connection to the
 * client and it provides functions for sending packets to the client.
 * If the client is logged in as a particular player, it contains a
 * reference to a PLAYER object and it contains a list of invitations
 * for which the client is either the source or the target.  CLIENT
 * objects are managed by the client registry.  So that a CLIENT
 * object can be passed around externally to the client registry
 * without fear of dangling references, it has a reference count that
 * corresponds to the number of references that exist to the object.
 * A CLIENT object will not be freed until its reference count reaches zero.
 */

/*
 * The CLIENT type is a structure type that defines the state of a client.
 * You will have to give a complete structure definition in client.c.
 * The precise contents are up to you.  Be sure that all the operations
 * that might be called concurrently are thread-safe.
 */
typedef struct client CLIENT;

#include "invitation.h"

/*
 * Create a new CLIENT object with a specified file descriptor with which
 * to communicate with the client.  The returned CLIENT has a reference
 * count of one and is in the logged-out state.
 *
 * @param creg  The client registry in which to create the client.
 * @param fd  File descriptor of a socket to be used for communicating
 * with the client.
 * @return  The newly created CLIENT object, if creation is successful,
 * otherwise NULL.
 */
CLIENT *client_create(CLIENT_REGISTRY *creg, int fd);

/*
 * Increase the reference count on a CLIENT by one.
 *
 * @param client  The CLIENT whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same CLIENT that was passed as a parameter.
 */
CLIENT *client_ref(CLIENT *client, char *why);

/*
 * Decrease the reference count on a CLIENT by one.  If after
 * decrementing, the reference count has reached zero, then the CLIENT
 * and its contents are freed.
 *
 * @param client  The CLIENT whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 */
void client_unref(CLIENT *client, char *why);

/*
 * Log in this CLIENT as a specified PLAYER.
 * The login fails if the CLIENT is already logged in or there is already
 * some other CLIENT that is logged in as the specified PLAYER.
 * Otherwise, the login is successful, the CLIENT is marked as "logged in"
 * and a reference to the PLAYER is retained by it.  In this case,
 * the reference count of the PLAYER is incremented to account for the
 * retained reference.
 *
 * @param CLIENT  The CLIENT that is to be logged in.
 * @param PLAYER  The PLAYER that the CLIENT is to be logged in as.
 * @return 0 if the login operation is successful, otherwise -1.
 */
int client_login(CLIENT *client, PLAYER *player);

/*
 * Log out this CLIENT.  If the client was not logged in, then it is
 * an error.  The reference to the PLAYER that the CLIENT was logged
 * in as is discarded, and its reference count is decremented.  Any
 * INVITATIONs in the client's list are revoked or declined, if
 * possible, any games in progress are resigned, and the invitations
 * are removed from the list of this CLIENT as well as its opponents'.
 *
 * @param client  The CLIENT that is to be logged out.
 * @return 0 if the client was logged in and has been successfully
 * logged out, otherwise -1.
 */
int client_logout(CLIENT *client);

/*
 * Get the PLAYER for the specified logged-in CLIENT.
 * The reference count on the returned PLAYER is NOT incremented,
 * so the returned reference should only be regarded as valid as long
 * as the CLIENT has not been freed.
 *
 * @param client  The CLIENT from which to get the PLAYER.
 * @return  The PLAYER that the CLIENT is currently logged in as,
 * otherwise NULL if the player is not currently logged in.
 */
PLAYER *client_get_player(CLIENT *client);

/*
 * Get the file descriptor for the network connection associated with
 * this CLIENT.
 *
 * @param client  The CLIENT for which the file descriptor is to be
 * obtained.
 * @return the file descriptor.
 */
int client_get_fd(CLIENT *client);

/*
 * Send a packet to a client.  Exclusive access to the network connection
 * is obtained for the duration of this operation, to prevent concurrent
 * invocations from corrupting each other's transmissions.  To prevent
 * such interference, only this function should be used to send packets to
 * the client, rather than the lower-level proto_send_packet() function.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @param pkt  The header of the packet to be sent.
 * @param data  Data payload to be sent, or NULL if none.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int client_send_packet(CLIENT *player, JEUX_PACKET_HEADER *pkt, void *data);

/*
 * Send an ACK packet to a client.  This is a convenience function that
 * streamlines a common case.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @param data  Pointer to the optional data payload for this packet,
 * or NULL if there is to be no payload.
 * @param datalen  Length of the data payload, or 0 if there is none.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int client_send_ack(CLIENT *client, void *data, size_t datalen);

/*
 * Send an NACK packet to a client.  This is a convenience function that
 * streamlines a common case.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int client_send_nack(CLIENT *client);

/*
 * Add an INVITATION to the list of outstanding invitations for a
 * specified CLIENT.  A reference to the INVITATION is retained by
 * the CLIENT and the reference count of the INVITATION is
 * incremented.  The invitation is assigned an integer ID,
 * which the client subsequently uses to identify the invitation.
 *
 * @param client  The CLIENT to which the invitation is to be added.
 * @param inv  The INVITATION that is to be added.
 * @return  The ID assigned to the invitation, if the invitation
 * was successfully added, otherwise -1.
 */
int client_add_invitation(CLIENT *client, INVITATION *inv);

/*
 * Remove an invitation from the list of outstanding invitations
 * for a specified CLIENT.  The reference count of the invitation is
 * decremented to account for the discarded reference.
 *
 * @param client  The client from which the invitation is to be removed.
 * @param inv  The invitation that is to be removed.
 * @return the CLIENT's id for the INVITATION, if it was successfully
 * removed, otherwise -1.
 */
int client_remove_invitation(CLIENT *client, INVITATION *inv);

/*
 * Make a new invitation from a specified "source" CLIENT to a specified
 * target CLIENT.  The invitation represents an offer to the target to
 * engage in a game with the source.  The invitation is added to both the
 * source's list of invitations and the target's list of invitations and
 * the invitation's reference count is appropriately increased.
 * An `INVITED` packet is sent to the target of the invitation.
 *
 * @param source  The CLIENT that is the source of the INVITATION.
 * @param target  The CLIENT that is the target of the INVITATION.
 * @param source_role  The GAME_ROLE to be played by the source of the INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of the INVITATION.
 * @return the ID assigned by the source to the INVITATION, if the operation
 * is successful, otherwise -1.
 */
int client_make_invitation(CLIENT *source, CLIENT *target,
			   GAME_ROLE source_role, GAME_ROLE target_role);

/*
 * Revoke an invitation for which the specified CLIENT is the source.
 * The invitation is removed from the lists of invitations of its source
 * and target CLIENT's and the reference counts are appropriately
 * decreased.  It is an error if the specified CLIENT is not the source
 * of the INVITATION, or the INVITATION does not exist in the source or
 * target CLIENT's list.  It is also an error if the INVITATION being
 * revoked is in a state other than the "open" state.  If the invitation
 * is successfully revoked, then the target is sent a REVOKED packet
 * containing the target's ID of the revoked invitation.
 *
 * @param client  The CLIENT that is the source of the invitation to be
 * revoked.
 * @param id  The ID assigned by the CLIENT to the invitation to be
 * revoked.
 * @return 0 if the invitation is successfully revoked, otherwise -1.
 */
int client_revoke_invitation(CLIENT *client, int id);

/*
 * Decline an invitation previously made with the specified CLIENT as target.  
 * The invitation is removed from the lists of invitations of its source
 * and target CLIENT's and the reference counts are appropriately
 * decreased.  It is an error if the specified CLIENT is not the target
 * of the INVITATION, or the INVITATION does not exist in the source or
 * target CLIENT's list.  It is also an error if the INVITATION being
 * declined is in a state other than the "open" state.  If the invitation
 * is successfully declined, then the source is sent a DECLINED packet
 * containing the source's ID of the declined invitation.
 *
 * @param client  The CLIENT that is the target of the invitation to be
 * declined.
 * @param id  The ID assigned by the CLIENT to the invitation to be
 * declined.
 * @return 0 if the invitation is successfully declined, otherwise -1.
 */
int client_decline_invitation(CLIENT *client, int id);

/*
 * Accept an INVITATION previously made with the specified CLIENT as
 * the target.  A new GAME is created and a reference to it is saved
 * in the INVITATION.  If the invitation is successfully accepted,
 * the source is sent an ACCEPTED packet containing the source's ID
 * of the accepted INVITATION.  If the source is to play the role of
 * the first player, then the payload of the ACCEPTED packet contains
 * a string describing the initial game state.  A reference to the
 * new GAME (with its reference count incremented) is returned to the
 * caller.
 *
 * @param client  The CLIENT that is the target of the INVITATION to be
 * accepted.
 * @param id  The ID assigned by the target to the INVITATION.
 * @param strp  Pointer to a variable into which will be stored either
 * NULL, if the accepting client is not the first player to move,
 * or a malloc'ed string that describes the initial game state,
 * if the accepting client is the first player to move.
 * If non-NULL, this string should be used as the payload of the `ACK`
 * message to be sent to the accepting client.  The caller must free
 * the string after use.
 * @return 0 if the INVITATION is successfully accepted, otherwise -1.
 */
int client_accept_invitation(CLIENT *client, int id, char **strp);

/*
 * Resign a game in progress.  This function may be called by a CLIENT
 * that is either source or the target of the INVITATION containing the
 * GAME that is to be resigned.  It is an error if the INVITATION containing
 * the GAME is not in the ACCEPTED state.  If the game is successfully
 * resigned, the INVITATION is set to the CLOSED state, it is removed
 * from the lists of both the source and target, and a RESIGNED packet
 * containing the opponent's ID for the INVITATION is sent to the opponent
 * of the CLIENT that has resigned.
 *
 * @param client  The CLIENT that is resigning.
 * @param id  The ID assigned by the CLIENT to the INVITATION that contains
 * the GAME to be resigned.
 * @return 0 if the game is successfully resigned, otherwise -1.
 */
int client_resign_game(CLIENT *client, int id);

/*
 * Make a move in a game currently in progress, in which the specified
 * CLIENT is a participant.  The GAME in which the move is to be made is
 * specified by passing the ID assigned by the CLIENT to the INVITATION
 * that contains the game.  The move to be made is specified as a string
 * that describes the move in a game-dependent format.  It is an error
 * if the ID does not refer to an INVITATION containing a GAME in progress,
 * if the move cannot be parsed, or if the move is not legal in the current
 * GAME state.  If the move is successfully made, then a MOVED packet is
 * sent to the opponent of the CLIENT making the move.  In addition, if
 * the move that has been made results in the game being over, then an
 * ENDED packet containing the appropriate game ID and the game result
 * is sent to each of the players participating in the game, and the
 * INVITATION containing the now-terminated game is removed from the lists
 * of both the source and target.  The result of the game is posted in
 * order to update both players' ratings.
 *
 * @param client  The CLIENT that is making the move.
 * @param id  The ID assigned by the CLIENT to the GAME in which the move
 * is to be made.
 * @param move  A string that describes the move to be made.
 * @return 0 if the move was made successfully, -1 otherwise.
 */
int client_make_move(CLIENT *client, int id, char *move);

#endif
