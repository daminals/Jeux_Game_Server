#include "includeme.h"

/*
 * A player registry maintains a mapping from usernames to PLAYER objects.
 * Entries persist for as long as the server is running.
 */

typedef struct player_node {
    PLAYER *player;
    struct player_node *next;
} PLAYER_NODE;

/*
 * The PLAYER_REGISTRY type is a structure type that defines the state
 * of a player registry.  You will have to give a complete structure
 * definition in player_registry.c. The precise contents are up to
 * you.  Be sure that all the operations that might be called
 * concurrently are thread-safe.
 */
typedef struct player_registry {
  pthread_mutex_t mutex;
  PLAYER_NODE *head;
  int length;
} PLAYER_REGISTRY;

/*
 * Initialize a new player registry.
 *
 * @return the newly initialized PLAYER_REGISTRY, or NULL if initialization
 * fails.
 */
PLAYER_REGISTRY *preg_init(void) {
  PLAYER_REGISTRY *preg = calloc(1,sizeof(PLAYER_REGISTRY));
  if (preg == NULL) {
    return NULL;
  }
  pthread_mutex_init(&preg->mutex, NULL);
  preg->head = NULL;
  preg->length = 0;
  return preg;
}

/*
 * Finalize a player registry, freeing all associated resources.
 *
 * @param cr  The PLAYER_REGISTRY to be finalized, which must not
 * be referenced again.
 */
void preg_fini(PLAYER_REGISTRY *preg) {
  pthread_mutex_lock(&preg->mutex);
  PLAYER_NODE *current = preg->head;
  while (current != NULL) {
    PLAYER_NODE *next = current->next;
    player_unref(current->player, "preg_fini");
    free(current);
    preg->length--;
    current = next;
  }
  pthread_mutex_unlock(&preg->mutex);
  pthread_mutex_destroy(&preg->mutex);
  free(preg);
}

/*
 * Register a player with a specified user name.  If there is already
 * a player registered under that user name, then the existing registered
 * player is returned, otherwise a new player is created.
 * If an existing player is returned, then its reference count is increased
 * by one to account for the returned pointer.  If a new player is
 * created, then the returned player has reference count equal to two:
 * one count for the pointer retained by the registry and one count for
 * the pointer returned to the caller.
 *
 * @param name  The player's user name, which is copied by this function.
 * @return A pointer to a PLAYER object, in case of success, otherwise NULL.
 *
 */
PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name) {
  pthread_mutex_lock(&preg->mutex);
  PLAYER_NODE *current = preg->head;
  PLAYER_NODE *last = NULL;
  while (current != NULL) {
    debug("preg_register: %s", player_get_name(current->player));
    if (strcmp(player_get_name(current->player), name) == 0) {
      pthread_mutex_unlock(&preg->mutex);
      player_ref(current->player, "preg_register");
      return current->player;
    }
    if (current->next == NULL) {
      last = current;
    }
    current = current->next;
  }
  PLAYER *player = player_create(name);
  if (player == NULL) {
    pthread_mutex_unlock(&preg->mutex);
    return NULL;
  }

  PLAYER_NODE *new_player = calloc(1, sizeof(PLAYER_NODE));
  if (new_player == NULL) {
    pthread_mutex_unlock(&preg->mutex);
    return NULL;
  }

  debug("player node created");
  new_player->player = player;
  new_player->next = NULL;
  if (last == NULL) {
    preg->head = new_player;
  } else {
    last->next = new_player;
  }
  preg->length++;
  pthread_mutex_unlock(&preg->mutex);
  player_ref(player, "returning new player in preg_register");
  return player;
}