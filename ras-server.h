/*
 * Copyright (C) 2020 Alexandre de Limas Santana <alexandre.delimassantana@bsc.es>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef __RAS_SERVER_H
#define __RAS_SERVER_H

#include <pthread.h>

#include "config.h"

/*
 * UNIX socket name, in abstract namespace, when broadcast is set.
 */
#define SOCKET_NAME "rasdaemon"
#define SERVER_MAX_CONN 10

struct ras_server {
  int socketfd;
  int *conn;
  int conn_size;
};

#ifdef HAVE_BROADCAST

/**
 * Create a local UNIX server for broadcasting RAS events to other processes
 **/
pthread_t server_begin(void);

/**
 * Broadcast a message to all processes connected to the server
 **/
void server_broadcast(const char *msg, size_t size);

#else

static inline pthread_t server_begin(void) { return -1; }
static inline void server_broadcast(const char *msg, size_t size) {}

#endif

#endif