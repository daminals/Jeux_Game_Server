#ifndef INCLUDEME_H
#define INCLUDEME_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <math.h>

#include "client_registry.h"
#include "player_registry.h"
#include "debug.h"
#include "jeux_globals.h"
#include "protocol.h"
#include "server.h"
#include "csapp.h"
extern JEUX_PACKET_HEADER *create_header(int type, int id, int role, int size);
#endif