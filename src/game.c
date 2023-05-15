#include "includeme.h"

/*
 * A GAME represents the current state of a game between participating
 * players.  So that a GAME object can be passed around
 * without fear of dangling references, it has a reference count that
 * corresponds to the number of references that exist to the object.
 * A GAME object will not be freed until its reference count reaches zero.
 */

/*
 * The GAME type is a structure type that defines the state of a game.
 * You will have to give a complete structure definition in game.c.
 * The precise contents are up to you.  Be sure that all the operations
 * that might be called concurrently are thread-safe.
 */
typedef struct game {
    int rows[9];
    int ref_count;
    int is_over;
    GAME_ROLE current_player;
    GAME_ROLE winner;
    pthread_mutex_t mutex;
} GAME;

/*
 * The GAME_MOVE type is a structure type that defines a move in a game.
 * The details are up to you.  A GAME_MOVE is immutable.
 */
typedef struct game_move {
    int move;
    GAME_ROLE role;
} GAME_MOVE;


/*
 * Create a new game in an initial state.  The returned game has a
 * reference count of one.
 *
 * @return the newly created GAME, if initialization was successful,
 * otherwise NULL.
 */
GAME *game_create(void) {
    GAME *game = calloc(1, sizeof(GAME));
    if (game == NULL) {
        return NULL;
    }
    for (int i = 0; i < 9; i++) {
        game->rows[i] = 0;
    }
    game->winner = NULL_ROLE;
    game->current_player = FIRST_PLAYER_ROLE;
    if (pthread_mutex_init(&game->mutex, NULL) != 0) {
        free(game);
        return NULL;
    }
    game->ref_count = 0;
    game->is_over = 0;
    game_ref(game, "game_create");
    return game;
}

/*
 * Increase the reference count on a game by one.
 *
 * @param game  The GAME whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same GAME object that was passed as a parameter.
 */
GAME *game_ref(GAME *game, char *why) {
    pthread_mutex_lock(&game->mutex);
    debug("Increase reference count on GAME %p (%d -> %d) for %s",
          game, game->ref_count, game->ref_count + 1, why);
    game->ref_count++;
    pthread_mutex_unlock(&game->mutex);
    return game;
}

/*
 * Decrease the reference count on a game by one.  If after
 * decrementing, the reference count has reached zero, then the
 * GAME and its contents are freed.
 *
 * @param game  The GAME whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 */
void game_unref(GAME *game, char *why) {
  pthread_mutex_lock(&game->mutex);
  debug("Decrease reference count on GAME %p (%d -> %d) for %s", game,
        game->ref_count, game->ref_count-1, why);
  game->ref_count--;
  if (game->ref_count == 0) {
    pthread_mutex_unlock(&game->mutex);
    pthread_mutex_destroy(&game->mutex);
    free(game);
    return;
  }
  pthread_mutex_unlock(&game->mutex);
} 

/*
 * Apply a GAME_MOVE to a GAME.
 * If the move is illegal in the current GAME state, then it is an error.
 *
 * @param game  The GAME to which the move is to be applied.
 * @param move  The GAME_MOVE to be applied to the game.
 * @return 0 if application of the move was successful, otherwise -1.
 */
int game_apply_move(GAME *game, GAME_MOVE *move) {
    pthread_mutex_lock(&game->mutex);
    if (game->current_player != move->role)
    {
        error("Player trying to move is not the current player");
        pthread_mutex_unlock(&game->mutex);
        return -1;
    }
    if (game->rows[move->move] != 0)
    {
        error("Move is not legal");
        pthread_mutex_unlock(&game->mutex);
        return -1;
    }
    game->rows[move->move] = move->role;
    game->current_player = (game->current_player == FIRST_PLAYER_ROLE) ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
    // check if the game is over
    pthread_mutex_unlock(&game->mutex);
    game_is_over(game);
    return 0;
}

/*
 * Submit the resignation of the GAME by the player in a specified
 * GAME_ROLE.  It is an error if the game has already terminated.
 *
 * @param game  The GAME to be resigned.
 * @param role  The GAME_ROLE of the player making the resignation.
 * @return 0 if resignation was successful, otherwise -1.
 */
int game_resign(GAME *game, GAME_ROLE role) {
    if (game->winner != NULL_ROLE)
    {
        error("Game has already terminated");
        return -1;
    }
    game->current_player = NULL_ROLE;
    game->is_over = 1;
    game->winner = (role == FIRST_PLAYER_ROLE) ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
    return 0;
}

/*
 * Get a string that describes the current GAME state, in a format
 * appropriate for human users.  The returned string is in malloc'ed
 * storage, which the caller is responsible for freeing when the string
 * is no longer required.
 *
 * @param game  The GAME for which the state description is to be
 * obtained.
 * @return  A string that describes the current GAME state.
 */
char *game_unparse_state(GAME *game) {
// X| | 
// -----
//  | | 
// -----
//  | | 
    char* game_state = calloc(1, sizeof(char) * 50);
    int current_place = 0;
    pthread_mutex_lock(&game->mutex);
    // row 1
    for (int i = 0; i < 3; i++) {
        if (game->rows[i] == 0) {
            game_state[current_place] = ' ';
        } else if (game->rows[i] == 1) {
            game_state[current_place] = 'X';
        } else {
            game_state[current_place] = 'O';
        }
        current_place++;
        if (i != 2) {
            game_state[current_place] = '|';
            current_place++;
        }
    }
    game_state[current_place] = '\n'; 
    current_place++;

    // create the floor
    for (int i = 0; i < 5; i++) {
        game_state[current_place] = '-';
        current_place++;
    }
    game_state[current_place] = '\n';
    current_place++;

    // row 2
    for (int i = 3; i < 6; i++) {
        if (game->rows[i] == 0) {
            game_state[current_place] = ' ';
        } else if (game->rows[i] == 1) {
            game_state[current_place] = 'X';
        } else {
            game_state[current_place] = 'O';
        }
        current_place++;
        if (i != 5) {
            game_state[current_place] = '|';
            current_place++;
        }
    }
    game_state[current_place] = '\n';
    current_place++;

    // create the floor
    for (int i = 0; i < 5; i++) {
        game_state[current_place] = '-';
        current_place++;
    }
    game_state[current_place] = '\n';
    current_place++;

    // row 3
    for (int i = 6; i < 9; i++) {
        if (game->rows[i] == 0) {
            game_state[current_place] = ' ';
        } else if (game->rows[i] == 1) {
            game_state[current_place] = 'X';
        } else {
            game_state[current_place] = 'O';
        }
        current_place++;
        if (i != 8) {
            game_state[current_place] = '|';
            current_place++;
        }
    }
    game_state[current_place] = '\n';
    current_place++;
    // X/0 to move
    if (game->current_player == 1) {
        game_state[current_place] = 'X';
    } else {
        game_state[current_place] = 'O';
    }
    pthread_mutex_unlock(&game->mutex);
    current_place++;
    char* to_move = " to move\n";
    for (int i = 0; i < strlen(to_move); i++) {
        game_state[current_place] = to_move[i];
        current_place++;
    }
    // end 
    game_state[current_place] = '\0';
    current_place++;
    game_state = realloc(game_state, sizeof(char) * current_place);
    return game_state;
}

/*
 * Determine if a specifed GAME has terminated.
 *
 * @param game  The GAME to be queried.
 * @return 1 if the game is over, 0 otherwise.
 */
int game_is_over(GAME *game) {
    pthread_mutex_lock(&game->mutex);
    // Check rows
    int* board = game->rows;
    for (int i = 0; i < 9; i += 3) {
        if (board[i] == board[i + 1] && board[i + 1] == board[i + 2] && board[i] != 0) {
            if (board[i] == FIRST_PLAYER_ROLE) {
              game->winner = FIRST_PLAYER_ROLE;
              game->is_over = 1;
              pthread_mutex_unlock(&game->mutex);
              return game->is_over;
            }
            else {
              game->winner = SECOND_PLAYER_ROLE;
              game->is_over = 1;
              pthread_mutex_unlock(&game->mutex);
             return game->is_over;
            }
        }
    }

    // Check columns
    for (int i = 0; i < 3; i++) {
        if (board[i] == board[i + 3] && board[i + 3] == board[i + 6] && board[i] != 0) {
            if (board[i] == FIRST_PLAYER_ROLE){
              game->winner = FIRST_PLAYER_ROLE;
              game->is_over = 1;
              pthread_mutex_unlock(&game->mutex);
              return game->is_over;
            }
            else {
              game->winner = SECOND_PLAYER_ROLE;
              game->is_over = 1;
              pthread_mutex_unlock(&game->mutex);
              return game->is_over;
            }
        }
    }

    // Check diagonals
    if (board[0] == board[4] && board[4] == board[8] && board[0] != 0)
    {
        if (board[0] == FIRST_PLAYER_ROLE) {
            game->winner = FIRST_PLAYER_ROLE;
            game->is_over = 1;
            pthread_mutex_unlock(&game->mutex);
            return game->is_over;
        }
        else {
            game->winner = SECOND_PLAYER_ROLE;
            game->is_over = 1;
            pthread_mutex_unlock(&game->mutex);
            return game->is_over;
        }
    }
    if (board[2] == board[4] && board[4] == board[6] && board[2] != 0) {
        if (board[2] == FIRST_PLAYER_ROLE) {
            game->winner = FIRST_PLAYER_ROLE;
            game->is_over = 1;
            pthread_mutex_unlock(&game->mutex);
            return game->is_over;
        }
        else {
            game->winner = SECOND_PLAYER_ROLE;
            game->is_over = 1;
            pthread_mutex_unlock(&game->mutex);
            return game->is_over;
        }
    }

    int empty_count = 0;
    for (int i = 0; i < 9; i++){
        if (board[i] == 0) {
            empty_count++;
        }
    }
    if (empty_count == 0){
        game->winner = NULL_ROLE;
        game->is_over = 1;
        pthread_mutex_unlock(&game->mutex);
        return game->is_over;
    }
    pthread_mutex_unlock(&game->mutex);
    return game->is_over;
}

/*
 * Get the GAME_ROLE of the player who has won the game.
 *
 * @param game  The GAME for which the winner is to be obtained.
 * @return  The GAME_ROLE of the winning player, if there is one.
 * If the game is not over, or there is no winner because the game
 * is drawn, then NULL_PLAYER is returned.
 */
GAME_ROLE game_get_winner(GAME *game) {
    return game->winner;
}

/*
 * Attempt to interpret a string as a move in the specified GAME.
 * If successful, a GAME_MOVE object representing the move is returned,
 * otherwise NULL is returned.  The caller is responsible for freeing
 * the returned GAME_MOVE when it is no longer needed.
 * Refer to the assignment handout for the syntax that should be used
 * to specify a move.
 *
 * @param game  The GAME for which the move is to be parsed.
 * @param role  The GAME_ROLE of the player making the move.
 * If this is not NULL_ROLE, then it must agree with the role that is
 * currently on the move in the game.
 * @param str  The string that is to be interpreted as a move.
 * @return  A GAME_MOVE described by the given string, if the string can
 * in fact be interpreted as a move, otherwise NULL.
 */
GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str) {
    GAME_MOVE* move = calloc(1, sizeof(GAME_MOVE));
    if (move == NULL) {
        return NULL;
    }
    move->role = role;
    // if string is in the format of single digit, use atoi
    // debug("str: %s", str);

    // string can be single char '1' - '9' or "\d<-<PLAYER_ROLE>"
    if (strlen(str) == 1) {
        move->move = atoi(str) - 1;
        if (move->move < 0 || move->move > 8) {
            error("INVALID MOVE: '%s' - move is not in the range 1-9", str);
            free(move);
            return NULL;
        }
        return move;
    }
    if (strlen(str) == 4) {
        if (str[1] != '<' || str[2] != '-') {
            free(move);
            return NULL;
        }
        if (str[3] == 'X' && game->current_player != FIRST_PLAYER_ROLE) {
           // fail
        error("INVALID MOVE: '%s' - X is not the current player", str);
            free(move);
            return NULL;
        }
        if (str[3] == 'O' && game->current_player != SECOND_PLAYER_ROLE) {
            // fail
            error("INVALID MOVE: '%s' - O is not the current player", str);
            free(move);
            return NULL;
        }
        move->move = atoi(str) - 1;
        if (move->move < 0 || move->move > 8) {
            free(move);
            error("INVALID MOVE: '%s' - move is not in the range 1-9", str);
            return NULL;
        }
        return move;
    }
    // fail
    error("INVALID MOVE: '%s' - NOT RECOGNIZED", str);
    free(move);
    return NULL;
    // // info("interpretation: %d", atoi(str)-1);
    // move->move = atoi(str) - 1;
    // if (move->move < 0 || move->move > 8) {
    //     free(move);
    //     return NULL;
    // }
    // return move;
}

/*
 * Get a string that describes a specified GAME_MOVE, in a format
 * appropriate to be shown to human users.  The returned string should
 * be in a format from which the GAME_MOVE can be recovered by applying
 * game_parse_move() to it.  The returned string is in malloc'ed storage,
 * which it is the responsibility of the caller to free when it is no
 * longer needed.
 *
 * @param move  The GAME_MOVE whose description is to be obtained.
 * @return  A string describing the specified GAME_MOVE.
 */
char *game_unparse_move(GAME_MOVE *move) {
    char* move_str = calloc(2, sizeof(char));
    if (move_str == NULL) {
        error("calloc failed");
        return NULL;
    }
    move_str[0] = move->move+1 + '0';
    move_str[1] = '\0';
    return move_str;
}