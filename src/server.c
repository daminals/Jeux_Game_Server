#include "csapp.h"
#include "includeme.h"
#include "jeux_globals.h"

// int send_nack_packet(int connfd) {
//   return send_packet(connfd, JEUX_NACK_PKT, NULL);
// }

// int send_ack_packet(int connfd, void** payload) {
//   return send_packet(connfd, JEUX_ACK_PKT, payload);
// }

// int send_packet(int connfd, JEUX_PACKET_TYPE type, void** payload) {
//   // create a header
//   // -------------------------------//
//   // uint8_t type;		              // Type of the packet
//   // uint8_t id;		            	  // Invitation ID
//   // uint8_t role;                  // Role of player in game
//   // uint16_t size;                 // Payload size (zero if no payload)
//   // uint32_t timestamp_sec;        // Seconds field of time packet was
//   // sent uint32_t timestamp_nsec;  // Nanoseconds field of time
//   // ------------------------------ //
//   JEUX_PACKET_HEADER *hdr_send = malloc(sizeof(JEUX_PACKET_HEADER));
//   hdr_send->type = type;
//   hdr_send->id = 0;

//   if (payload != NULL) {
//     hdr_send->size = htons(strlen((char*)payload));
//   } else {
//     hdr_send->size = 0;
//   }

//   hdr_send->role = 0;
//   // get time and put it in header
//   struct timespec ts;
//   clock_gettime(CLOCK_MONOTONIC, &ts);
//   hdr_send->timestamp_nsec = htonl(ts.tv_nsec);
//   hdr_send->timestamp_sec = htonl(ts.tv_sec);
//   // send packet
//   proto_send_packet(connfd, hdr_send, payload);
//   free(hdr_send);
//   return 0;
// }

int process_login(char* payload, int connfd, JEUX_PACKET_HEADER *hdr, CLIENT *client) {
  // ONLY FREE WHATEVER IS DEFINED HERE!
  debug("RECIEVED PACKET (clientfd=%d, type=LOGIN) for client %p at %d", connfd, client, ntohl(hdr->timestamp_sec));
  // check if client is already logged in:
  if (client_get_player(client) != NULL) {
    debug("client already logged in");
    client_send_nack(client);
    return -1;
  }
  // check if payload is null
  if (payload == NULL) {
    debug("payload is null");
    client_send_nack(client);
    return -1;
  }
  // calloc username
  char *username = calloc(ntohs(hdr->size)+1, sizeof(char));
  strncpy(username, (char*)payload, ntohs(hdr->size));
  // debug("username: %s", username);

  // check if payload is empty string
  if (strlen((char*)payload) == 0) {
    debug("payload is empty string");
    client_send_nack(client);
    return -1;
  }
  // check if username is already taken
  warn("username: %s", username);
  // preg reg should do this teehee
  // CLIENT* c = creg_lookup(client_registry, username);
  // if (c != NULL) {
  //   debug("username already taken");
  //   client_unref(c, "find if username is taken - it is lol");
  //   free(username);
  //   client_send_nack(client);
  //   return -1;
  // }

  // payload is username
  PLAYER *new_player = preg_register(player_registry, username);
  debug("survived preg_register: %p", new_player);
  if (new_player == NULL) {
    debug("player registry is full");
    client_send_nack(client);
    free(username);
    return -1;
  }
  // debug("player name: %s", player_get_name(new_player));
  if (client_login(client, new_player) == -1)  {
    error("Failed to log in client");
    client_send_nack(client);
  }
  player_unref(new_player, "register new player");
  // send ack packet
  client_send_ack(client, NULL, 0);
  free(username);
  return 1;
}

int process_users(char* payload, int connfd, CLIENT *client) {
  debug("RECIEVED PACKET (clientfd=%d, type=USERS) for client %p", connfd, client);
  // payload is null
  if (payload != NULL) {
    debug("payload is not null");
    client_send_nack(client);
    return -1;
  }

  // print all users
  info("printing all users");
  char* send_payload = NULL;
  // i do the open memstream
  size_t send_size = 0;
  FILE* stream = open_memstream(&send_payload, &send_size);
  // get array of all players, save inital pointer
  PLAYER **players = creg_all_players(client_registry);
  PLAYER **players_copy = players;
  while (*players != NULL) {
    // player name + 4 digit ELO + 1 tab + 1 newline + \0
    int needed_size = strlen(player_get_name(*players)) + 4 + 2 + 1;
    char temp_buffer[needed_size];
    // initialize the buffer
    memset(temp_buffer, 0, needed_size);
    sprintf(temp_buffer, "%s\t%d\n", player_get_name(*players), player_get_rating(*players));
    fwrite(temp_buffer, sizeof(char), strlen(temp_buffer), stream);
    player_unref(*players, "users packet in jeux_client_service (unref from creg_all_players)");
    players++;
  }
  // flush memstream to send_payload
  fflush(stream);
  // close memstream
  fclose(stream);
  // free players array (reference counts already decremented)
  free(players_copy);
  // send ack packet
  debug("strlen: %ld send_payload: %s", strlen(send_payload), send_payload);
  client_send_ack(client, (void*) send_payload, strlen(send_payload)); 
  free(send_payload);
  return 0;
}

int process_invite(char* payload, int connfd, CLIENT *client, JEUX_PACKET_HEADER *hdr) {
  debug("RECIEVED PACKET (clientfd=%d, type=INVITE) for client %p", connfd, client);
  // check if payload is null
  if (payload == NULL) {
    debug("payload is null");
    client_send_nack(client);
    return -1;
  }
  // payload is user who i am inviting to play with me
  //                                                    payload is of kind soul who invited me to a blessed match
  // payload not gauranteed null terminated
  // char* username = calloc(ntohs(hdr->size)+1, sizeof(char));
  // strcpy(username, (char*)payload);
  // find player who sent invite 
  CLIENT *invitee = creg_lookup(client_registry, payload);
  if (invitee == NULL) {
    debug("invitee is null");
    // free(username);
    client_send_nack(client);
    return -1;
  }
  // decrement creg ref count
  client_unref(invitee, "find invitee");

  // did i just invite myself owo
  if (client == invitee) {
    debug("client is invitee");
    // free(username);
    client_send_nack(client);
    return -1;
  }

  // im blushing uwu
  info("sending invite to %s", payload);
  // what role am i?
  int their_role = hdr->role;
  int invitation_id = -1;
  if (their_role == FIRST_PLAYER_ROLE) {
    invitation_id = client_make_invitation(client, invitee, SECOND_PLAYER_ROLE,FIRST_PLAYER_ROLE);
  } else if (their_role == SECOND_PLAYER_ROLE) {
    invitation_id = client_make_invitation(client, invitee, FIRST_PLAYER_ROLE,SECOND_PLAYER_ROLE);
  } else {
    debug("my_role is not 1 or 2");
    // free(username);
    client_send_nack(client);
    return -1;
  }
  // alright qt, i accept this packet uwu
  // create a header so i can send an ack packet with the invitation id
  JEUX_PACKET_HEADER* send_hdr = create_header(JEUX_ACK_PKT, invitation_id, 0, 0);
  client_send_packet(client, send_hdr, NULL);
  free(send_hdr);
  // free(username);
  return 0;
  // end
}

int process_revoke(char* payload, int connfd, CLIENT *client, JEUX_PACKET_HEADER *hdr) {
  debug("RECIEVED PACKET (clientfd=%d, type=REVOKE) for client %p", connfd, client);
  // check that no payload
  if (payload != NULL) {
    debug("payload is not null");
    client_send_nack(client);
    return -1;
  }
  // get the id of the invitation to revoke
  int invitation_id = hdr->id;
  warn("invitation_id: %d", invitation_id);
  // revoke the invitation
  int revoke_result = client_revoke_invitation(client, invitation_id);
  if (revoke_result == -1) {
    debug("revoke_result == -1");
    client_send_nack(client);
    return -1;
  }
  // send ack packet
  client_send_ack(client, NULL, 0);
  return 0;
}
int process_decline(char* payload, int connfd, CLIENT *client, JEUX_PACKET_HEADER *hdr) {
  debug("RECIEVED PACKET (clientfd=%d, type=DECLINE) for client %p", connfd, client);
  // check that no payload
  if (payload != NULL) {
    debug("payload is not null");
    client_send_nack(client);
    return -1;
  }
  // get the id of the invitation to decline
  int invitation_id = hdr->id;
  warn("invitation_id: %d", invitation_id);
  // decline the invitation
  int decline_result = client_decline_invitation(client, invitation_id);
  if (decline_result == -1) {
    debug("decline_result == -1");
    client_send_nack(client);
    return -1;
  }
  // send ack packet
  client_send_ack(client, NULL, 0);
  return 0;
}

int process_accept(char* payload, int connfd, CLIENT *client, JEUX_PACKET_HEADER *hdr) {
  debug("RECIEVED PACKET (clientfd=%d, type=ACCEPT) for client %p", connfd, client);
  // check that no payload
  if (payload != NULL) {
    debug("payload is not null");
    client_send_nack(client);
    return -1;
  }
  // get the id of the invitation to accept
  int invitation_id = hdr->id;
  warn("invitation_id: %d", invitation_id);
  // accept the invitation inv_accept
  char* game_state = NULL;
  // what the fuck is this string pointer ???? HUH where do i get game state
  int accept_result = client_accept_invitation(client, invitation_id, &game_state);
  // debug("game_state: %s", *game_state);
  if (accept_result == -1) {
    debug("accept_result == -1");
    client_send_nack(client);
    return -1;
  }
  warn("ack packet");
  // send ack packet
  if (game_state == NULL) {
    client_send_ack(client, NULL, 0);
  } else {
    client_send_ack(client, game_state, strlen(game_state));
    free(game_state);
  }

  // if (hdr->role == SECOND_PLAYER_ROLE) {
  // } else {
  // }
  return 0;
}

int process_move(char* payload, int connfd, CLIENT *client, JEUX_PACKET_HEADER *hdr) {
  debug("RECIEVED PACKET (clientfd=%d, type=MOVE, id=%d, role=%d) for client %p", connfd, hdr->id, hdr->role, client);
  // check if payload is null
  if (payload == NULL) {
    debug("payload is null");
    client_send_nack(client);
    return -1;
  }
  int invitation_id = hdr->id;
  warn("invitation_id: %d", invitation_id);
  // payload is move
  // payload not gauranteed null terminated
  char* move = calloc(ntohs(hdr->size)+1, sizeof(char));
  strncpy(move, (char*)payload, ntohs(hdr->size));
  // make move
  int move_result = client_make_move(client, invitation_id, move);
  if (move_result == -1) {
    debug("move_result == -1");
    free(move);
    client_send_nack(client);
    return -1;
  }
  // send ack packet
  client_send_ack(client, NULL, 0);
  free(move);
  return 0;
}

int process_resign(char* payload, int connfd, CLIENT *client, JEUX_PACKET_HEADER *hdr) {
  debug("RECIEVED PACKET (clientfd=%d, type=RESIGN) for client %p", connfd, client);
  // check that no payload
  if (payload != NULL) {
    debug("payload is not null");
    client_send_nack(client);
    return -1;
  }
  int invitation_id = hdr->id;
  warn("invitation_id: %d", invitation_id);
  // resign
  int resign_result = client_resign_game(client, invitation_id);
  if (resign_result == -1) {
    debug("resign_result == -1");
    client_send_nack(client);
    return -1;
  }
  // send ack packet
  client_send_ack(client, NULL, 0);
  return 0;
}

/*
 * Thread function for the thread that handles a particular client.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 * @return  NULL
 *
 * This function executes a "service loop" that receives packets from
 * the client and dispatches to appropriate functions to carry out
 * the client's requests.  It also maintains information about whether
 * the client has logged in or not.  Until the client has logged in,
 * only LOGIN packets will be honored.  Once a client has logged in,
 * LOGIN packets will no longer be honored, but other packets will be.
 * The service loop ends when the network connection shuts down and
 * EOF is seen.  This could occur either as a result of the client
 * explicitly closing the connection, a timeout in the network causing
 * the connection to be closed, or the main thread of the server shutting
 * down the connection as part of graceful termination.
 */
void *jeux_client_service(void *arg) {
  int connfd = *((int *)arg);
  CLIENT *client = creg_register(client_registry, connfd);
  free(arg);
  pthread_detach(pthread_self());
  if (client == NULL) {
    debug("client == NULL");
    Close(connfd);
    return NULL;
  }

  debug("thread: %d", connfd);
  int process_login_packet = 0;
  int cont = 1;
  while (cont) {
    // make header
    // JEUX_PACKET_HEADER *hdr = calloc(1,sizeof(JEUX_PACKET_HEADER));
    JEUX_PACKET_HEADER full_header = {0}, *hdr = &full_header;
    void *payload = NULL;
    int ret = proto_recv_packet(connfd, hdr, &payload);
    if (ret == -1) {
      client_logout(client);
      cont = 0;
      break;
    }
    // payload is not null terminated so we need to do this
    if (payload != NULL) {
      char* payload_str = calloc(ntohs(hdr->size)+1, sizeof(char));
      strncpy(payload_str, (char*)payload, ntohs(hdr->size));
      free(payload);
      payload = payload_str;
      debug("payload: %s", (char*)payload);
    }
    // process stuff in header and payload
    switch (hdr->type) {
      case JEUX_LOGIN_PKT:
        process_login_packet = process_login(payload, connfd, hdr, client);
        break;
      case JEUX_USERS_PKT:
        if (process_login_packet == 0) {
          debug("process_login_packet == 0");
          client_send_nack(client);
        } else {
          process_users(NULL, connfd, client);
        }
        break;
      case JEUX_INVITE_PKT:
        if (process_login_packet == 0) {
          debug("process_login_packet == 0");
          client_send_nack(client);
        } else {
          process_invite(payload, connfd, client, hdr);
        }
        break;
      case JEUX_REVOKE_PKT:
        if (process_login_packet == 0) {
          debug("process_login_packet == 0");
          client_send_nack(client);
        } else {
          info("made it here :>)");
          process_revoke(NULL, connfd, client, hdr); 
        }
        break;
      case JEUX_ACCEPT_PKT:
        if (process_login_packet == 0) {
          debug("process_login_packet == 0");
          client_send_nack(client);
        } else {
          process_accept(NULL, connfd, client, hdr);
        }
        break;
      case JEUX_DECLINE_PKT:
        if (process_login_packet == 0) {
          debug("process_login_packet == 0");
          client_send_nack(client);
        } else {
          process_decline(NULL, connfd, client, hdr);
        }
        break;
      case JEUX_MOVE_PKT:
        if (process_login_packet == 0) {
          debug("process_login_packet == 0");
          client_send_nack(client);
        } else {
          process_move(payload, connfd, client, hdr);
        }
        break;
      case JEUX_RESIGN_PKT:
        if (process_login_packet == 0) {
          debug("process_login_packet == 0");
          client_send_nack(client);
        } else {
          process_resign(NULL, connfd, client, hdr);
        }
        break;
      case JEUX_NO_PKT:
        cont = 0;
        client_logout(client);
        break;
      default:
        debug("default");
        // if (process_login == 0) {
        //   debug("process_login == 0");
        // client_send_nack(client);
        //   break;
        // }
        break;
    }
    if (payload != NULL) {
      debug("payload: %s", (char *)payload);
      free(payload);
      payload = NULL;
    }
    // free(hdr);
  }

  // unregister client
  creg_unregister(client_registry, client);

  Close(connfd);
  debug("Connection closed by client");
  return NULL;
}