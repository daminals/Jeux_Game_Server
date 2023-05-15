#include "includeme.h"
#include "debug.h"


typedef struct client_registry {
  pthread_mutex_t mutex;  
  sem_t sem;
  int length;
  // reject new clients after shutdown
  int no;
  CLIENT* clients[MAX_CLIENTS];
  sem_t login_queue;
} CLIENT_REGISTRY;

/*
 * Initialize a new client registry.
 *
 * @return the newly initialized client registry, or NULL if initialization
 * fails.
 */
CLIENT_REGISTRY *creg_init() {
  CLIENT_REGISTRY *cr = calloc(1, sizeof(CLIENT_REGISTRY));
  if (cr == NULL) {
    return NULL;
  }
  int result = pthread_mutex_init(&cr->mutex, NULL);
  if (result != 0) {
    free(cr);
    return NULL;
  }
  if (sem_init(&cr->sem, 0, 0) != 0) {
    pthread_mutex_destroy(&cr->mutex);
    free(cr);
    return NULL;
  }
  if (sem_init(&cr->login_queue, 0, MAX_CLIENTS) != 0) {
    pthread_mutex_destroy(&cr->mutex);
    sem_destroy(&cr->sem);
    free(cr);
    return NULL;
  }
  cr->length = 0;
  // initialize clients to NULL
  for (int i = 0; i < MAX_CLIENTS; i++) {
    cr->clients[i] = NULL;
  }
  cr->no = 0;
  info("Client registry initialized");
  return cr;
}

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr) {
  debug("creg is fini :'(");
  sem_destroy(&cr->sem);
  sem_destroy(&cr->login_queue);
  pthread_mutex_destroy(&cr->mutex);
  free(cr);
  return;
}

/*
 * Register a client file descriptor.
 * If successful, returns a reference to the the newly registered CLIENT,
 * otherwise NULL.  The returned CLIENT has a reference count of one.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 * @return a reference to the newly registered CLIENT, if registration
 * is successful, otherwise NULL.
 */
CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd) {
  if (cr == NULL) {
    return NULL;
  }
  if (cr->length >= MAX_CLIENTS) {
    return NULL;
  }
  if (cr->no == 1) {
    debug("No new clients allowed ☝️☝️☝️");
    return NULL;
  }
  // wait for login queue
  sem_wait(&cr->login_queue);
  // add client to registry
  pthread_mutex_lock(&cr->mutex);
  // error("Banning login again (sem_post: %d)", cr->length);
  // create client
  CLIENT *client = client_create(cr, fd);
  // add client to registry
  cr->clients[cr->length] = client;
  debug("Increment Registry Length (%d -> %d)", cr->length, cr->length+1);
  cr->length++;
  // unlock mutex
  pthread_mutex_unlock(&cr->mutex);
  return client;
}

/*
 * Unregister a CLIENT, removing it from the registry.
 * The client reference count is decreased by one to account for the
 * pointer discarded by the client registry.  If the number of registered
 * clients is now zero, then any threads that are blocked in
 * creg_wait_for_empty() waiting for this situation to occur are allowed
 * to proceed.  It is an error if the CLIENT is not currently registered
 * when this function is called.
 *
 * @param cr  The client registry.
 * @param client  The CLIENT to be unregistered.
 * @return 0  if unregistration succeeds, otherwise -1.
 */
int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client) {
  // loop through cr until you find client
  if (cr == NULL || client == NULL) {
    return -1;
  }
  pthread_mutex_lock(&cr->mutex);
  for (int i = 0; i < cr->length; i++) {
    if (cr->clients[i] == client) {
      // remove client from registry
      cr->clients[i] = NULL;
      // move all clients after it up one
      for (int j = i; j < cr->length - 1; j++) {
        cr->clients[j] = cr->clients[j + 1];
      }
      client_logout(client);
      client_unref(client, "unregister");
      debug("Decrement Registry Length (%d -> %d)", cr->length, cr->length-1);
      cr->length--;
      sem_post(&cr->login_queue); // someone logged out you can login now
      error("Allowed to login again (sem_post: %d)", cr->length);
      // unlock mutex
      if (cr->length == 0) {
        info("No more clients, releasing sem");
        // decrease reference count of client
        sem_post(&cr->sem);
      }
      pthread_mutex_unlock(&cr->mutex);
      return 0;
    }
  }
  pthread_mutex_unlock(&cr->mutex);
  return -1;
}

/*
 * Given a username, return the CLIENT that is logged in under that
 * username.  The reference count of the returned CLIENT is
 * incremented by one to account for the reference returned.
 *
 * @param cr  The registry in which the lookup is to be performed.
 * @param user  The username that is to be looked up.
 * @return the CLIENT currently registered undjer the specified
 * username, if there is one, otherwise NULL.
 */
CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user) {
  if (cr == NULL) {
    return NULL;
  }
  pthread_mutex_lock(&cr->mutex);
  // loop through cr until you find client
  for (int i = 0; i < cr->length; i++) {
    // skip NULL clients
    if (cr->clients[i] == NULL) {
      continue;
    }
    // skip clients that aren't logged in
    PLAYER* player = client_get_player(cr->clients[i]);
    if (player == NULL) {
      continue;
    }
    // compare usernames
    if (strcmp(player_get_name(player), user) == 0) {
      // increase reference count of client
      client_ref(cr->clients[i], "lookup");
      warn("client found: %d, %s", i, player_get_name(player));
      pthread_mutex_unlock(&cr->mutex);
      return cr->clients[i];
    }
  }
  pthread_mutex_unlock(&cr->mutex);
  return NULL;
}

/*
 * Return a list of all currently logged in players.  The result is
 * returned as a malloc'ed array of PLAYER pointers, with a NULL
 * pointer marking the end of the array.  It is the caller's
 * responsibility to decrement the reference count of each of the
 * entries and to free the array when it is no longer needed.
 *
 * @param cr  The registry for which the set of usernames is to be
 * obtained.
 * @return the list of players as a NULL-terminated array of pointers.
 */
PLAYER **creg_all_players(CLIENT_REGISTRY *cr) {
  // lock mutex
  pthread_mutex_lock(&cr->mutex);

  // create array of players
  PLAYER **players = calloc((cr->length + 1), sizeof(PLAYER*));
  if (players == NULL) {
    return NULL;
  }
  int length = 0;
  // loop through cr until you find a logged in player
  for (int i = 0; i < cr->length; i++) {
    if (cr->clients[i] == NULL) {
      continue;
    }
    PLAYER* player_logged_in = client_get_player(cr->clients[i]);
    if (player_logged_in != NULL) {
      // increase reference count of player
      player_ref(player_logged_in, "all_players array function");
      info("set player %s to index %d", player_get_name(player_logged_in), i);
      players[length++] = player_logged_in;
    }
  }
  players = realloc(players, sizeof(PLAYER*) * (length + 1));
  debug("length: %d", length);
  players[length] = NULL;
  // unlock mutex
  pthread_mutex_unlock(&cr->mutex);
  return players;
}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
    // Wait for the mutex lock
    
    // If there are clients registered, wait on the semaphore
    debug("waiting on %d clients", cr->length);
    if (cr->length > 0) {
      // int* sem_value = malloc(sizeof(int));
      // sem_getvalue(&cr->sem, sem_value);
      // info("waiting for empty: sem: %d", *sem_value);
      // free(sem_value);
      sem_wait(&cr->sem);
    }
    
    // Unlock the mutex
}

/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The clients are not unregistered
 * by this function.  It is intended that the clients will be
 * unregistered by the threads servicing their connections, once
 * those server threads have recognized the EOF on the connection
 * that has resulted from the socket shutdown.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr) {
  pthread_mutex_lock(&cr->mutex);
  cr->no = 1;
  for (int i=0; i<cr->length; i++) {
    if (cr->clients[i] != NULL) {
      shutdown(client_get_fd(cr->clients[i]), SHUT_RD);
    }
  }
  pthread_mutex_unlock(&cr->mutex);
  return;
}
