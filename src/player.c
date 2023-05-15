#include "includeme.h"

#include <stdio.h>

/*
 * A PLAYER represents a user of the system.  A player has a username,
 * which does not change, and also has a "rating", which is a value
 * that reflects the player's skill level among all players known to
 * the system.  The player's rating changes as a result of each game
 * in which the player participates.  PLAYER objects are managed by
 * the player registry.  So that a PLAYER object can be passed around
 * externally to the player registry without fear of dangling
 * references, it has a reference count that corresponds to the number
 * of references that exist to the object.  A PLAYER object will not
 * be freed until its reference count reaches zero.
 */

/*
 * The PLAYER type is a structure type that defines the state of a player.
 * You will have to give a complete structure definition in player.c.
 * The precise contents are up to you.  Be sure that all the operations
 * that might be called concurrently are thread-safe.
 */
typedef struct player {
  char* name;
  int ref_count;
  int rating;
  pthread_mutex_t mutex;  
} PLAYER;

// sem_t post_result_sem;
// int init_post_result_sem = 0;

/* The initial rating assigned to a player. */

/*
 * Create a new PLAYER with a specified username.  A private copy is
 * made of the username that is passed.  The newly created PLAYER has
 * a reference count of one, corresponding to the reference that is
 * returned from this function.
 *
 * @param name  The username of the PLAYER.
 * @return  A reference to the newly created PLAYER, if initialization
 * was successful, otherwise NULL.
 */
PLAYER *player_create(char *name) {
  // if (init_post_result_sem == 0) {
  //   sem_init(&post_result_sem, 0, 1);
  // }
  PLAYER* player = calloc(1, sizeof(PLAYER));
  if (player == NULL) {
    return NULL;
  }
  pthread_mutex_init(&player->mutex, NULL);
  player->name = strdup(name);
  player->ref_count = 0;
  player->rating = PLAYER_INITIAL_RATING;
  player_ref(player, "instantiated player object");
  return player;
}

/*
 * Increase the reference count on a player by one.
 *
 * @param player  The PLAYER whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same PLAYER object that was passed as a parameter.
 */
PLAYER *player_ref(PLAYER *player, char *why) {
  pthread_mutex_lock(&player->mutex);
  debug("Increase reference count on player %p [%s] (%d -> %d) for %s",
        player, player->name, player->ref_count, player->ref_count + 1, why);
  player->ref_count++;
  pthread_mutex_unlock(&player->mutex);
  return player;
}

/*
 * Decrease the reference count on a PLAYER by one.
 * If after decrementing, the reference count has reached zero, then the
 * PLAYER and its contents are freed.
 *
 * @param player  The PLAYER whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
void player_unref(PLAYER *player, char *why) {
  pthread_mutex_lock(&player->mutex);
  debug("Decrease reference count on player %p [%s] (%d -> %d) for %s",
        player, player->name, player->ref_count, player->ref_count - 1, why);
  player->ref_count--;
  if (player->ref_count == 0) {
    debug("Freeing player %p [%s]", player, player->name);
    pthread_mutex_unlock(&player->mutex);
    pthread_mutex_destroy(&player->mutex);
    free(player->name);
    free(player);
    return;
  }
  pthread_mutex_unlock(&player->mutex);
}

/*
 * Get the username of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the username of the player.
 */
char *player_get_name(PLAYER *player) {
  return player->name;
}

/*
 * Get the rating of a player.
 *
 * @param player  The PLAYER that is to be queried.
 * @return the rating of the player.
 */
int player_get_rating(PLAYER *player) {
  return player->rating;
}

/*
 * Post the result of a game between two players.
 * To update ratings, we use a system of a type devised by Arpad Elo,
 * similar to that used by the US Chess Federation.
 * The player's ratings are updated as follows:
 * Assign each player a score of 0, 0.5, or 1, according to whether that
 * player lost, drew, or won the game.
 * Let S1 and S2 be the scores achieved by player1 and player2, respectively.
 * Let R1 and R2 be the current ratings of player1 and player2, respectively.
 * Let E1 = 1/(1 + 10**((R2-R1)/400)), and
 *     E2 = 1/(1 + 10**((R1-R2)/400))
 * Update the players ratings to R1' and R2' using the formula:
 *     R1' = R1 + 32*(S1-E1)
 *     R2' = R2 + 32*(S2-E2)
 *
 * @param player1  One of the PLAYERs that is to be updated.
 * @param player2  The other PLAYER that is to be updated.
 * @param result   0 if draw, 1 if player1 won, 2 if player2 won.
 */
void player_post_result(PLAYER *player1, PLAYER *player2, int result) {
  // can only post results one at a time
  // sem_wait(&post_result_sem);
  if (player1 == NULL || player2 == NULL) {
    return;
  }

  if (player1 == player2) {
    return;
  }

  if (result < 0 || result > 2) {
    return;
  }

  pthread_mutex_lock(&player1->mutex);
  pthread_mutex_lock(&player2->mutex);
  // double E1;// E2; 
  double S1 = 0.0;
  double S2 = 0.0; 
  // double R1, R2 = PLAYER_INITIAL_RATING;

  switch (result) {
    case 0:
      S1 = S2 = 0.5;
      break;
    case 1:
      S1 = 1.0;
      break;
    case 2:
      S2 = 1.0;
      break;
  }
  
  // R1 = player1->rating;
  // R2 = player2->rating;
  
  int R3 = player1->rating + player2->rating;
  
  double E1 = 1.0 / (1.0 + pow(10.0, ((player2->rating - player1->rating) / 400.0))); // 1/(1 + 10**((R2-R1)/400))
  // E2 = 1.0 / (1.0 + pow(10, ((R1 - R2) / 400.0)));

  double RP1 = player1->rating + 32 * (S1 - E1);
  // double RP2 = R2 + 32 * (S2 - E2);
  // debug("Player %s (rating %d) vs. Player %s (rating %d), result %d\nNew ratings: %s: %f, %s: %f",
  //       player_get_name(player1), player_get_rating(player1),
  //       player_get_name(player2), player_get_rating(player2), result, player_get_name(player1), RP1, player_get_name(player2), RP2);
  player1->rating = RP1;
  player2->rating = R3 - RP1;

  pthread_mutex_unlock(&player1->mutex);
  pthread_mutex_unlock(&player2->mutex);
  // sem_post(&post_result_sem);
  return;
}