#include "includeme.h"
#include "jeux_globals.h"

// mine
#include "option_processing.h"

#ifdef DEBUG
int _debug_packets_ = 1;

#endif

// int close_listen_socket = 0;
// int listen_socket = -1;
static void terminate(int status);

/* INCLUDED IN HW4 SUBMISSION */

// handle signals
#define HANDLE_SIGHUP 0x1
static int cont_running = 1;
volatile int signal_received = 0x0;
struct sigaction sighandler;
sigset_t mask;
sigset_t sec_mask;

void sighup_handler(int signum, siginfo_t* siginfo, void* context) {
  switch (signum) {
    // watched_signals[0]
    case SIGHUP:
      debug("SIGHUP received");
      signal_received |= HANDLE_SIGHUP;
      // if (close_listen_socket) {
      //   debug("Closing listen socket");
      //   Close(listen_socket);
      // }
      // terminate(EXIT_SUCCESS);
      break;
    case SIGINT:
      debug("SIGINT received");
      #ifdef DEBUG
      signal_received |= HANDLE_SIGHUP;
      // terminate(EXIT_SUCCESS);
      #endif
    default:
      debug("Signal %d received", signum);
      break;
  }
}


void setup_signal_handler() {
  // Set up signal handler for SIGINT
  sighandler.sa_sigaction = sighup_handler;
  sigemptyset(&sighandler.sa_mask);
  sighandler.sa_flags = SA_SIGINFO;
  // Wait for SIGACTION and execute do_something()
  sigemptyset(&mask);
  if (sigaction(SIGHUP, &sighandler, NULL) == -1) {
    debug("sigaction: %d", SIGHUP);
    exit(EXIT_FAILURE);
  }
  #ifdef DEBUG
  if (sigaction(SIGINT, &sighandler, NULL) == -1) {
    debug("sigaction: %d", SIGHUP);
    exit(EXIT_FAILURE);
  }
  #endif
  // mask contains the set of signals to be listened to
  if (sigaddset(&mask, SIGHUP) == -1) {
    debug("sigaddset: %d", SIGHUP);
    exit(EXIT_FAILURE);
  }
  #ifdef DEBUG
  if (sigaddset(&mask, SIGINT) == -1) {
    debug("sigaddset: %d", SIGHUP);
    exit(EXIT_FAILURE);
  }
  #endif
}

/* END OF HW4 CODE SUBMISSION */


/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]) {
  // Option processing should be performed here.
  // Option '-p <port>' is required in order to specify the port number
  // on which the server should listen.
  if (option_processor(argc, argv)) {
    fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  debug("pid: %d", getpid());
  setup_signal_handler();
  // Perform required initializations of the client_registry and
  // player_registry.
  client_registry = creg_init();
  player_registry = preg_init();

  // TODO: Set up the server socket and enter a loop to accept connections
  // on this socket.  For each connection, a thread should be started to
  // run function jeux_client_service().  In addition, you should install
  // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
  // shutdown of the server.

  // textbook code
  int* connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; /* Enough space for any address */

  int listenfd = Open_listenfd(PORT);
  // listen_socket = listenfd;
  // close_listen_socket = 1;
  while (cont_running) {
    debug("Waiting for client connection...");

    clientlen = sizeof(struct sockaddr_storage);
    connfd = calloc(1, sizeof(int));
    *connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);

    if (signal_received & HANDLE_SIGHUP) {
      debug("SIGHUP received");
      cont_running = 0;
      free(connfd);
      Close(listenfd);
      terminate(EXIT_SUCCESS);
      break;
    }

    debug("this socket is connected: %d", *connfd);
    pthread_t tid;
    // debug("Client connected");
    if (pthread_create(&tid, NULL, jeux_client_service, connfd) != 0) {
      error("pthread_create");
      exit(EXIT_FAILURE);
    }
  }

  // fprintf(stderr, "You have to finish implementing main() "
  //   "before the Jeux server will function.\n");

  terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
  // Shutdown all client connections.
  // This will trigger the eventual termination of service threads.
  creg_shutdown_all(client_registry);

  debug("%ld: Waiting for service threads to terminate...", pthread_self());
  creg_wait_for_empty(client_registry);
  debug("%ld: All service threads terminated.", pthread_self());

  // Finalize modules.
  creg_fini(client_registry);
  preg_fini(player_registry);

  debug("%ld: Jeux server terminating", pthread_self());
  // check if listen fd is an open file descriptor:
  exit(status);
}
