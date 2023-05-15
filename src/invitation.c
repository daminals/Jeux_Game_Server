#include "includeme.h"
/*
 * An INVITATION records the status of an offer, made by one CLIENT
 * to another, to participate in a GAME.  The CLIENT that initiates
 * the offer is called the "source" of the invitation, and the CLIENT
 * that is the recipient of the offer is called the "target" of the
 * invitation.  An INVITATION stores references to its source and
 * target CLIENTs, whose reference counts reflect the existence of
 * these references.  At any time, an INVITATION may be in one of
 * three states: OPEN, ACCEPTED, or CLOSED.  A newly created INVITATION
 * starts out in the OPEN state.  An OPEN invitation may be "accepted"
 * or "declined" by its target.  It may also be "revoked" by its
 * source.  An invitation that has been accepted by its target transitions
 * to the ACCEPTED state.  In association with such a transition a new
 * GAME is created and a reference to it is stored in the INVITATION.
 * An invitation that is declined by its target or revoked by its
 * source transitions to the CLOSED state.  An invitation in the
 * ACCEPTED state will also transition to the CLOSED state when the
 * game in progress has ended.
 */

/*
 * The INVITATION type is a structure type that defines the state of
 * an invitation.  You will have to give a complete structure
 * definition in invitation.c.  The precise contents are up to you.
 * Be sure that all the operations that might be called concurrently
 * are thread-safe.
 */
typedef struct invitation {
  INVITATION_STATE state;
  int ref_count;
  CLIENT *source;
  CLIENT *target;
  GAME *game;
  GAME_ROLE source_role;
  GAME_ROLE target_role;
  pthread_mutex_t lock;
} INVITATION;

/*
 * Create an INVITATION in the OPEN state, containing reference to
 * specified source and target CLIENTs, which cannot be the same CLIENT.
 * The reference counts of the source and target are incremented to reflect
 * the stored references.
 *
 * @param source  The CLIENT that is the source of this INVITATION.
 * @param target  The CLIENT that is the target of this INVITATION.
 * @param source_role  The GAME_ROLE to be played by the source of this INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of this INVITATION.
 * @return a reference to the newly created INVITATION, if initialization
 * was successful, otherwise NULL.
 */
INVITATION *inv_create(CLIENT *source, CLIENT *target,
    GAME_ROLE source_role, GAME_ROLE target_role) {
    INVITATION* inv = calloc(1, sizeof(INVITATION));
    if (inv == NULL) {
      error("cowlick invite failed");
      return NULL;
    }
    inv->state = INV_OPEN_STATE;
    inv->ref_count = 0;
    inv_ref(inv, "newly created invite 😄✌️");
    inv->source = source;
    client_ref(source, "source of invite");
    inv->target = target;
    client_ref(target, "target of invite");
    inv->source_role = source_role;
    inv->target_role = target_role;

    int result = pthread_mutex_init(&inv->lock, NULL);
    if (result != 0) {
      free(inv);
      error("mutex init invite failed");
      return NULL;
    }
    return inv;
  }

/*
 * Increase the reference count on an invitation by one.
 *
 * @param inv  The INVITATION whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same INVITATION object that was passed as a parameter.
 */
INVITATION *inv_ref(INVITATION *inv, char *why) {
  pthread_mutex_lock(&inv->lock);
  debug("Increase reference count on invitation %p (%d -> %d) for %s",
        inv, inv->ref_count, inv->ref_count + 1, why);
  inv->ref_count++;
  pthread_mutex_unlock(&inv->lock);
  return inv;
}

/*
 * Decrease the reference count on an invitation by one.
 * If after decrementing, the reference count has reached zero, then the
 * invitation and its contents are freed.
 *
 * @param inv  The INVITATION whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
void inv_unref(INVITATION *inv, char *why) {
  pthread_mutex_lock(&inv->lock);
  debug("Decrease reference count on invitation %p (%d -> %d) for %s",
        inv, inv->ref_count, inv->ref_count - 1, why);
  inv->ref_count--;
  if (inv->ref_count == 0) {
    pthread_mutex_unlock(&inv->lock);
    pthread_mutex_destroy(&inv->lock);
    if (inv->game != NULL) {
      game_unref(inv->game, "invite game");
    }
    // remove ref counts of source and target
    client_unref(inv->source, "remove reference of source of invite");
    client_unref(inv->target, "remove reference of target of invite");
    free(inv);
    return;
  }
  pthread_mutex_unlock(&inv->lock);
  // return inv;
}

/*
 * Get the CLIENT that is the source of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid as
 * long as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the source of the INVITATION.
 */
CLIENT *inv_get_source(INVITATION *inv) {
  if (inv == NULL) return NULL;
  return inv->source;
}

/*
 * Get the CLIENT that is the target of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid if
 * the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the target of the INVITATION.
 */
CLIENT *inv_get_target(INVITATION *inv) {
  if (inv == NULL) return NULL;
  return inv->target;
}

/*
 * Get the GAME_ROLE to be played by the source of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the source of the INVITATION.
 */
GAME_ROLE inv_get_source_role(INVITATION *inv) {
  return inv->source_role;
}

/*
 * Get the GAME_ROLE to be played by the target of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the target of the INVITATION.
 */
GAME_ROLE inv_get_target_role(INVITATION *inv) {
  return inv->target_role;
}

/*
 * Get the GAME (if any) associated with an INVITATION.
 * The reference count of the returned GAME is NOT incremented,
 * so the GAME reference should only be regarded as valid as long
 * as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME associated with the INVITATION, if there is one,
 * otherwise NULL.
 */
GAME *inv_get_game(INVITATION *inv) {
  if (inv == NULL) return NULL;
  return inv->game;
}

/*
 * Accept an INVITATION, changing it from the OPEN to the
 * ACCEPTED state, and creating a new GAME.  If the INVITATION was
 * not previously in the the OPEN state then it is an error.
 *
 * @param inv  The INVITATION to be accepted.
 * @return 0 if the INVITATION was successfully accepted, otherwise -1.
 */
int inv_accept(INVITATION *inv) {
  if (inv == NULL) return -1;
  if (inv->state != INV_OPEN_STATE){
    error("inv_accept: invitation not in open state");
    return -1;
  }
  inv->state = INV_ACCEPTED_STATE;
  inv->game = game_create();
  if (inv->game == NULL) {
    error("inv_accept: game create failed");
    return -1;
  }
  return 0;
}

/*
 * Close an INVITATION, changing it from either the OPEN state or the
 * ACCEPTED state to the CLOSED state.  If the INVITATION was not previously
 * in either the OPEN state or the ACCEPTED state, then it is an error.
 * If INVITATION that has a GAME in progress is closed, then the GAME
 * will be resigned by a specified player.
 *
 * @param inv  The INVITATION to be closed.
 * @param role  This parameter identifies the GAME_ROLE of the player that
 * should resign as a result of closing an INVITATION that has a game in
 * progress.  If NULL_ROLE is passed, then the invitation can only be
 * closed if there is no game in progress.
 * @return 0 if the INVITATION was successfully closed, otherwise -1.
 */
int inv_close(INVITATION *inv, GAME_ROLE role) {
  if (inv == NULL) {
    error("inv is null");
    return -1;
  }
  if (inv->state != INV_OPEN_STATE && inv->state != INV_ACCEPTED_STATE) {
    error("inv_close: invitation not in open or accepted state");
    return -1;
  }
  if (inv->state == INV_ACCEPTED_STATE) {
    if (inv->game == NULL) {
      error("inv_close: invitation in accepted state but game is null");
      return -1;
    }
    if (role == NULL_ROLE) {
      error("inv_close: invitation in accepted state but role is null");
      return -1;
    }
    game_resign(inv->game, role);
  }
  if (inv->state == INV_OPEN_STATE) {
    if (role != NULL_ROLE) {
      error("inv_close: invitation in open state but role is not null");
      return -1;
    }
    if (inv->game != NULL) {
      error("inv_close: invitation in open state but game is not null");
      return -1;
    }
  }
  inv->state = INV_CLOSED_STATE;
  return 0;
}
