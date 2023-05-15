/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef CLIENT_REGISTRY_H
#define CLIENT_REGISTRY_H

/*
 * A client registry keeps track of the clients that are currently connected.
 * Each time a client connects, a new CLIENT object is created and added to
 * the registry.  When the thread servicing a client is about to terminate,
 * it removes the CLIENT object from the registry.  The client registry also
 * provides a function for shutting down all client connections and a function
 * that can be called by a thread that wishes to wait for the client count to
 * drop to zero.  Such a function is useful, for example, in order to achieve
 * clean server termination: when termination is desired, the "main" thread
 * will shut down all client connections and then wait for the set of registered
 * clients become empty before exiting the program.
 */

/*
 * The maximum number of simultaneous clients supported by the registry.
 */
#define MAX_CLIENTS 64

/*
 * The CLIENT_REGISTRY type is a structure that defines the state of a
 * client registry.  You will have to give a complete structure
 * definition in client_registry.c.  The precise contents are up to
 * you.  Be sure that all the operations that might be called
 * concurrently are thread-safe.
 */
typedef struct client_registry CLIENT_REGISTRY;

#include "client.h"

/*
 * Initialize a new client registry.
 *
 * @return  the newly initialized client registry, or NULL if initialization
 * fails.
 */
CLIENT_REGISTRY *creg_init();

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr);

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
CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd);

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
int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client);

/*
 * Given a username, return the CLIENT that is logged in under that
 * username.  The reference count of the returned CLIENT is
 * incremented by one to account for the reference returned.
 *
 * @param cr  The registry in which the lookup is to be performed.
 * @param user  The username that is to be looked up.
 * @return the CLIENT currently registered under the specified
 * username, if there is one, otherwise NULL.
 */
CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user);

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
PLAYER **creg_all_players(CLIENT_REGISTRY *cr);

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr);

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
void creg_shutdown_all(CLIENT_REGISTRY *cr);

#endif
