#include "includeme.h"

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

typedef struct invitation_node {
  struct invitation_node *next;
  int id;
  INVITATION *invitation;
} INVITATION_NODE;

// however many functions need their own semmy
#define CLIENT_SEM_FUNCTIONS 10
#define CLIENT_LOGIN_SEM 0
#define CLIENT_LOGOUT_SEM 1
#define CLIENT_INVITE_SEM 2
// #define CLIENT_USE_NETWORK_SEM 3
#define CLIENT_ID_NUM 4
#define CLIENT_REF 5
#define CLIENT_INVITE_OP_SEM 6
#define CLIENT_GET_INVITE_ID_SEM 7
#define CLIENT_UPDATE_ELO 8

// semaphores for all invitation functions
sem_t semaphores[CLIENT_SEM_FUNCTIONS];
int is_sem_init = 0;

typedef enum client_state {
  CLIENT_LOGGED_OUT,
  CLIENT_LOGGED_IN,
} CLIENT_STATE;

/*
 * The CLIENT type is a structure type that defines the state of a client.
 * You will have to give a complete structure definition in client.c.
 * The precise contents are up to you.  Be sure that all the operations
 * that might be called concurrently are thread-safe.
 */
typedef struct client {
  pthread_mutex_t lock;
  CLIENT_REGISTRY *cr;
  int fd;
  int ref_count;
  PLAYER *player;
  CLIENT_STATE logged_in;
  int *available_ids;
  int id_usage;
  int current_id_size;
  INVITATION_NODE *invite_head;
} CLIENT;

/*
 * Create a new CLIENT object with a specified file descriptor with which
 * to communicate with the client.  The returned CLIENT has a reference
 * count of one and is in the logged-out state.
 *
 * @param creg  The client registry in which to create the client.
 * @param fd  File descriptor of a socket to be used for communicating
 * with the client.
 * @return  The newly created CLIENT objectlo, if creation is successful,
 * otherwise NULL.
 */
CLIENT *client_create(CLIENT_REGISTRY *creg, int fd) {
  if (is_sem_init == 0) {
    info("Initializing semaphores");
    for (int i = 0; i < CLIENT_SEM_FUNCTIONS; i++) {
      if (sem_init(&semaphores[i], 0, 1) != 0) {
        error("did not init sem correctly");
        return NULL;
      }
    }
  }

  CLIENT *client = calloc(1, sizeof(CLIENT));
  debug("Creating new client on fd: %d", fd);
  if (client == NULL) {
    error("did not cowlick correctly");
    return NULL;
  }
  client->current_id_size = 100;
  client->available_ids = calloc(client->current_id_size, sizeof(int));
  if (client->available_ids == NULL) {
    error("did not cowlick available_ids correctly");
    return NULL;
  }
  // info("Initializing available_ids");
  for (int i = 0; i < client->current_id_size; i++) {
    client->available_ids[i] = i;
  }
  int ret = pthread_mutex_init(&client->lock, NULL);
  if (ret != 0) {
    error("did not init mutex correctly");
    return NULL;
  }
  // info("Initializing client");
  client->fd = fd;
  client->logged_in = CLIENT_LOGGED_OUT;
  client->ref_count = 0;
  client->id_usage = 0;
  client->cr = creg;
  client->player = NULL;
  client->invite_head = NULL;
  client_ref(client, "client_create");
  return client;
}

int post_player_results(CLIENT *client, CLIENT *opponent, GAME_ROLE client_role, GAME_ROLE winner) {
  sem_wait(&semaphores[CLIENT_UPDATE_ELO]);
  // post results
  PLAYER *player1 = NULL;
  PLAYER *player2 = NULL;

  if (client_role == FIRST_PLAYER_ROLE) {
    player1 = client_get_player(client);
    player2 = client_get_player(opponent);
  } else {
    player1 = client_get_player(opponent);
    player2 = client_get_player(client);
  }
  if (player1 == NULL || player2 == NULL) {
    error("Failed to get players");
    sem_post(&semaphores[CLIENT_UPDATE_ELO]);
    return -1;
  }
  
  player_post_result(player1, player2, winner);
  sem_post(&semaphores[CLIENT_UPDATE_ELO]);
  return 0;
}

int get_available_id(CLIENT *client) {
  // sem_wait(&semaphores[CLIENT_ID_NUM]);
  pthread_mutex_lock(&client->lock);
  int id = -1;
  if (client->id_usage >= client->current_id_size) {
    client->current_id_size *= 2;
    client->available_ids =
        realloc(client->available_ids, client->current_id_size * sizeof(int));
    if (client->available_ids == NULL) {
      error("did not cowlick available_ids correctly");
      return -1;
    }
    for (int i = client->current_id_size / 2; i < client->current_id_size;
         i++) {
      client->available_ids[i] = i;
    }
  }

  for (int i = 0; i < client->current_id_size; i++) {
    if (client->available_ids[i] == -1) {
      continue;
    }
    id = client->available_ids[i];
    client->available_ids[i] = -1;
    client->id_usage++;
    break;
  }
  // sem_post(&semaphores[CLIENT_ID_NUM]);
  pthread_mutex_unlock(&client->lock);
  return id;
}

void purge_id(CLIENT *client, int id) {
  // sem_wait(&semaphores[CLIENT_ID_NUM]);
  pthread_mutex_lock(&client->lock);
  if (id >= client->current_id_size) {
    error("id is out of bounds");
    return;
  }
  client->available_ids[id] = id;
  client->id_usage--;
  pthread_mutex_unlock(&client->lock);
  // sem_post(&semaphores[CLIENT_ID_NUM]);
}

int client_get_invitation_id(CLIENT *client, INVITATION *inv) {
  pthread_mutex_lock(&client->lock);
  INVITATION_NODE *node = client->invite_head;
  if (node == NULL) {
    pthread_mutex_unlock(&client->lock);
    return -1;
  }
  while (node != NULL) {
    if (node->invitation == inv) {
      int id = node->id;
      pthread_mutex_unlock(&client->lock);
      return id;
    }
    node = node->next;
  }
  pthread_mutex_unlock(&client->lock);
  return -1;
}

INVITATION *client_get_invitation(CLIENT *client, int id) {
  pthread_mutex_lock(&client->lock);
  INVITATION_NODE *node = client->invite_head;
  if (node == NULL) {
    error("Client has no invitations");
    pthread_mutex_unlock(&client->lock);
    return NULL;
  }
  while (node != NULL) {
    if (node->id == id) {
      INVITATION *inv = node->invitation;
      pthread_mutex_unlock(&client->lock);
      return inv;
    }
    node = node->next;
  }
  pthread_mutex_unlock(&client->lock);
  error("Invitation was not found");
  return NULL;
}

/*
 * Increase the reference count on a CLIENT by one.
 *
 * @param client  The CLIENT whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same CLIENT that was passed as a parameter.
 */
CLIENT *client_ref(CLIENT *client, char *why) {
  pthread_mutex_lock(&client->lock);
  debug("Increase reference count on client %p (%d -> %d) for %s", client,
        client->ref_count, client->ref_count + 1, why);
  client->ref_count++;
  pthread_mutex_unlock(&client->lock);
  return client;
}

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
void client_unref(CLIENT *client, char *why) {
  pthread_mutex_lock(&client->lock);
  debug("Decrease reference count on client %p (%d -> %d) for %s", client,
        client->ref_count, client->ref_count-1, why);
  client->ref_count--;
  pthread_mutex_unlock(&client->lock);
  if (client->ref_count == 0) {
    if (client->logged_in == CLIENT_LOGGED_IN) {
      client_logout(client);
    }
    free(client->available_ids);
    pthread_mutex_destroy(&client->lock);
    // for (int i = 0; i < CLIENT_SEM_FUNCTIONS; i++) {
    //   sem_destroy(&semaphores[i]);
    // }
    // close(client->fd);
    // do not close fd, that is done in server
    free(client);
  }
}

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
int client_login(CLIENT *client, PLAYER *player) {
  sem_wait(&semaphores[CLIENT_LOGIN_SEM]);
  // check if client is already logged in
  if (client->logged_in != CLIENT_LOGGED_OUT) {
    error("client is already logged in");
    return -1;
  }

  // check if player is already logged in
  PLAYER **players = creg_all_players(client->cr);
  PLAYER **players_copy = players;
  char *name = player_get_name(player);
  int logged_in = 0;
  while (*players != NULL) {
    if (strcmp(player_get_name(*players), name) == 0) {
      debug("player already logged in");
      logged_in = 1;
    }
    player_unref(*players, "searching if player is already logged in");
    players++;
  }
  free(players_copy);
  if (logged_in) {
    error("Player is currently logged in");
    sem_post(&semaphores[CLIENT_LOGIN_SEM]);
    return -1;
  }
  // player is not logged in, so log in
  pthread_mutex_lock(&client->lock);
  client->player = player;
  player_ref(player, "Player is now referenced by client after login");
  pthread_mutex_unlock(&client->lock);
  client->logged_in = CLIENT_LOGGED_IN;
  sem_post(&semaphores[CLIENT_LOGIN_SEM]);
  return 0;
}

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
int client_logout(CLIENT *client) {
  // check if not logged in
  if (client->logged_in != CLIENT_LOGGED_IN) {
    debug("client is not logged in");
    return -1;
  }
  info("Client [%s] is logging out", player_get_name(client->player));
  // lock semaphore
  sem_wait(&semaphores[CLIENT_LOGOUT_SEM]);
  // revoke or decline invitations
  INVITATION_NODE *node = client->invite_head;
  while (node != NULL) {
    // resign or decline all games
    INVITATION *inv = node->invitation;
    GAME* game = inv_get_game(inv);
    INVITATION_NODE *next = node->next;
    if (game != NULL) {
      client_resign_game(client, node->id);
    } else {
      if (inv_get_source(inv) == client) {
        client_revoke_invitation(client, node->id);
      } else {
        client_decline_invitation(client, node->id);
      }
    }
    node = next;
  }
  player_unref(client->player, "client logging out of player -> removing player reference");
  // unlock semaphore
  client->logged_in = CLIENT_LOGGED_OUT;
  sem_post(&semaphores[CLIENT_LOGOUT_SEM]);
  return 0;
}

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
PLAYER *client_get_player(CLIENT *client) {
  if (client == NULL) {
    return NULL;
  }
  return client->player;
}

/*
 * Get the file descriptor for the network connection associated with
 * this CLIENT.
 *
 * @param client  The CLIENT for which the file descriptor is to be
 * obtained.
 * @return the file descriptor.
 */
int client_get_fd(CLIENT *client) { return client->fd; }

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
int client_send_packet(CLIENT *player, JEUX_PACKET_HEADER *pkt, void *data) {
  pthread_mutex_lock(&player->lock);
  int ret = proto_send_packet(player->fd, pkt, data);
  pthread_mutex_unlock(&player->lock);
  return ret;
}

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
int client_send_ack(CLIENT *client, void *data, size_t datalen) {
  pthread_mutex_lock(&client->lock);
  JEUX_PACKET_HEADER *pkt = create_header(JEUX_ACK_PKT, 0, 0, datalen);
  int ret = proto_send_packet(client->fd, pkt, data);
  free(pkt);
  pthread_mutex_unlock(&client->lock);
  return ret;
}

/*
 * Send an NACK packet to a client.  This is a convenience function that
 * streamlines a common case.
 *
 * @param client  The CLIENT who should be sent the packet.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int client_send_nack(CLIENT *client) {
  pthread_mutex_lock(&client->lock);
  JEUX_PACKET_HEADER *pkt = create_header(JEUX_NACK_PKT, 0, 0, 0);
  int ret = proto_send_packet(client->fd, pkt, NULL);
  free(pkt);
  pthread_mutex_unlock(&client->lock);
  return ret;
}

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
int client_add_invitation(CLIENT *client, INVITATION *inv) {
  sem_wait(&semaphores[CLIENT_INVITE_SEM]);
  INVITATION_NODE *node = calloc(1, sizeof(INVITATION_NODE));
  // always assign the lowest available ID
  node->id = get_available_id(client);
  node->invitation = inv;
  inv_ref(inv, "add invitation to client (client_add_invitation function)");
  node->next = client->invite_head;
  pthread_mutex_lock(&client->lock);
  client->invite_head = node;
  pthread_mutex_unlock(&client->lock);
  sem_post(&semaphores[CLIENT_INVITE_SEM]);
  return node->id;
}

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
int client_remove_invitation(CLIENT *client, INVITATION *inv) {
  sem_wait(&semaphores[CLIENT_INVITE_SEM]);
  INVITATION_NODE *node = client->invite_head;

  if (node == NULL) {
    sem_post(&semaphores[CLIENT_INVITE_SEM]);
    return -1;
  }
  INVITATION_NODE *prev = NULL;
  while (node != NULL) {
    if (node->invitation == inv) {
      if (prev == NULL) {
        client->invite_head = node->next;
      } else {
        prev->next = node->next;
      }
      inv_unref(
          inv,
          "remove invitation from client (client_remove_invitation function)");
      int id = node->id;
      purge_id(client, id);
      free(node);
      sem_post(&semaphores[CLIENT_INVITE_SEM]);
      return id;
    }
    prev = node;
    node = node->next;
  }
  sem_post(&semaphores[CLIENT_INVITE_SEM]);
  return -1;
}

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
 * @param source_role  The GAME_ROLE to be played by the source of the
 * INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of the
 * INVITATION.
 * @return the ID assigned by the source to the INVITATION, if the operation
 * is successful, otherwise -1.
 */
int client_make_invitation(CLIENT *source, CLIENT *target,
                           GAME_ROLE source_role, GAME_ROLE target_role) {
  sem_wait(&semaphores[CLIENT_INVITE_OP_SEM]);
  INVITATION *invite = inv_create(source, target, source_role, target_role);
  if (invite == NULL) {
    error("invitation creation failed");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  int source_id = client_add_invitation(source, invite);
  if (source_id == -1) {
    inv_unref(invite, "invitation add failed");
    error("invitation add failed");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  int target_id = client_add_invitation(target, invite);
  if (target_id == -1) {
    error("invitation add failed");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  // send invited packet
  char *playername = player_get_name(source->player);
  JEUX_PACKET_HEADER *pkt = create_header(JEUX_INVITED_PKT, target_id,
                                          target_role, strlen(playername));
  if (client_send_packet(target, pkt, playername) == -1) {
    error("Failed to send invited packet");
  }
  free(pkt);
  inv_unref(invite, "Invitation made (client_make_invitation function)");
  sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
  return source_id;
}

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
int client_revoke_invitation(CLIENT *client, int id) {
  sem_wait(&semaphores[CLIENT_INVITE_OP_SEM]);
  INVITATION *inv = client_get_invitation(client, id);
  if (inv_close(inv, NULL_ROLE) == -1) {
    error("Cannot close invitation");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  // get target
  CLIENT *target = inv_get_target(inv);
  // get target inv id
  int target_id = client_get_invitation_id(target, inv);
  // send revoked packet
  JEUX_PACKET_HEADER *pkt = create_header(JEUX_REVOKED_PKT, target_id, 0, 0);
  if (client_send_packet(target, pkt, NULL) == -1) {
    free(pkt);
    error("Failed to send revoked packet");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  free(pkt);
  // remove from both lists
  debug("Removing invitation from source and target (client_revoke_invitation function)");
  if (client_remove_invitation(client, inv) == -1) {
    error("Failed to remove invitation from source");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  if (client_remove_invitation(target, inv) == -1) {
    error("Failed to remove invitation from target");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }

  sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
  return 0;
}

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
int client_decline_invitation(CLIENT *client, int id) {
  sem_wait(&semaphores[CLIENT_INVITE_OP_SEM]);
  INVITATION *inv = client_get_invitation(client, id);
  if (inv_close(inv, NULL_ROLE) == -1) {
    error("Cannot close invitation");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  // get source
  CLIENT *source = inv_get_source(inv);
  // get source inv id
  int source_id = client_get_invitation_id(source, inv);
  // send revoked packet
  JEUX_PACKET_HEADER *pkt = create_header(JEUX_DECLINED_PKT, source_id, 0, 0);
  if (client_send_packet(source, pkt, NULL) == -1) {
    error("Failed to send declined packet");
  }
  free(pkt);
  // remove from both lists
  info("Removing invitation from source and target (client_decline_invitation function)");
  if (client_remove_invitation(client, inv) == -1) {
    error("Failed to remove invitation from target");
  }
  if (client_remove_invitation(source, inv) == -1) {
    error("Failed to remove invitation from source");
  }

  sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
  return 0;
}

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
int client_accept_invitation(CLIENT *client, int id, char **strp) {
  sem_wait(&semaphores[CLIENT_INVITE_OP_SEM]);
  INVITATION *inv = client_get_invitation(client, id);

  if (inv_get_target(inv) != client) {
    error("Source cannot accept invitation");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }

  if (inv_accept(inv) == -1) {
    error("Cannot accept invitation");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  // get current game state
  GAME *game = inv_get_game(inv);
  // get source
  CLIENT *source = inv_get_source(inv);
  // get source inv id
  int source_id = client_get_invitation_id(source, inv);
  // check if client is first person or second person to play
  int source_length = 0;
  char *source_game_state = NULL;
  GAME_ROLE role = inv_get_target_role(inv);
  char* game_state = game_unparse_state(game);
  if (role != FIRST_PLAYER_ROLE) {
    *strp = NULL;
    source_game_state = game_state;
    source_length = strlen(source_game_state);
  } else {
    // set string pointer to game state
    *strp = game_state;
    source_game_state = NULL;
  }
  // send accepted packet
  JEUX_PACKET_HEADER *pkt =
      create_header(JEUX_ACCEPTED_PKT, source_id, 0, source_length);
  if (client_send_packet(source, pkt, source_game_state) == -1) {
    error("Failed to send accepted packet");
    free(game_state);
    free(pkt);
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  free(pkt);
  if (source_game_state != NULL) {
    free(source_game_state);
  }
  // game state is not freed if in strp, (caller responsibility)
  // otherwise it is freed
  sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
  return 0;
}

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
int client_resign_game(CLIENT *client, int id) {
  sem_wait(&semaphores[CLIENT_INVITE_OP_SEM]);
  INVITATION *inv = client_get_invitation(client, id);
  GAME_ROLE role = NULL_ROLE;
  GAME_ROLE opp_role = NULL_ROLE;
  CLIENT *opponent = NULL;
  if (client == inv_get_source(inv)) {
    role = inv_get_source_role(inv);
    opponent = inv_get_target(inv);
    opp_role = inv_get_target_role(inv);
  } else if (client == inv_get_target(inv)) {
    role = inv_get_target_role(inv);
    opponent = inv_get_source(inv);
    opp_role = inv_get_source_role(inv);
  } else {
    error("Client is not source or target of invitation");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  // cannot resign a game that is in the open state, and not accepted state

  if (inv_close(inv, role) == -1) {
    error("Failed to close invitation (client resign game)");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  // get opponent inv id
  int opponent_id = client_get_invitation_id(opponent, inv);
  // send resigned packet
  JEUX_PACKET_HEADER *pkt = create_header(JEUX_RESIGNED_PKT, opponent_id, 0, 0);
  if (client_send_packet(opponent, pkt, NULL)) {
    error("Failed to send resigned packet");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  free(pkt);

  // update results from resigning
  if (post_player_results(client, opponent, role, opp_role) == -1) {
    error("Failed to post player results");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }

  // remove from both lists
  info("Removing invitation from source and target (client resign game)");
  if (client_remove_invitation(client, inv) == -1) {
    error("Failed to remove invitation from source");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  if (client_remove_invitation(opponent, inv) == -1) {
    error("Failed to remove invitation from target");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }

  sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
  return 0;
}

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
int client_make_move(CLIENT *client, int id, char *move) {
  sem_wait(&semaphores[CLIENT_INVITE_OP_SEM]);
  INVITATION *inv = client_get_invitation(client, id);
  GAME *game = inv_get_game(inv);
  if (game == NULL) {
    error("Cannot make move in game that does not exist");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  GAME_ROLE role = NULL_ROLE;
  CLIENT *opponent = NULL;
  if (client == inv_get_source(inv)) {
    role = inv_get_source_role(inv);
    opponent = inv_get_target(inv);
  } else if (client == inv_get_target(inv)) {
    role = inv_get_target_role(inv);
    opponent = inv_get_source(inv);
  } else {
    error("Client is not source or target of invitation");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  debug("MOVE: %s", move);
  GAME_MOVE *game_move = game_parse_move(game, role, move);
  if (game_move == NULL) {
    error("Failed to parse move");
    free(game_move);
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  if (game_apply_move(game, game_move) == -1) {
    error("Failed to apply move");
    free(game_move);
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  free(game_move);
  // get game state string
  char *state = game_unparse_state(game);
  // get opponent inv id
  int opponent_id = client_get_invitation_id(opponent, inv);
  // send moved packet
  JEUX_PACKET_HEADER *pkt =
      create_header(JEUX_MOVED_PKT, opponent_id, 0, strlen(state));
  if (client_send_packet(opponent, pkt, state) == -1) {
    error("Failed to send moved packet");
    free(pkt);
    free(state);
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  free(pkt);
  free(state);
  // check if game is over
  if (!game_is_over(game)) {
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return 0;
  }
  warn("Detected GAME OVER (client make move)");
  // get winner
  GAME_ROLE winner = game_get_winner(game);
  // send ended packet
  pkt = create_header(JEUX_ENDED_PKT, opponent_id, winner, 0);
  if (client_send_packet(opponent, pkt, NULL) == -1) {
    error("Failed to send ended packet");
    free(pkt);
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  pkt->id = id;
  if (client_send_packet(client, pkt, NULL) == -1) {
    error("Failed to send ended packet");
    free(pkt);
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  free(pkt);
  // post results
  post_player_results(client, opponent, role, winner);
  // remove invite from both lists
  info("Removing invitation from source and target (client make move)");
  if (client_remove_invitation(client, inv) == -1) {
    error("Failed to remove invitation from source");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  if (client_remove_invitation(opponent, inv) == -1) {
    error("Failed to remove invitation from target");
    sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
    return -1;
  }
  sem_post(&semaphores[CLIENT_INVITE_OP_SEM]);
  return 0;
}
