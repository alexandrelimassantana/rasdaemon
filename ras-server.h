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

#include "config.h"
#include "ras-events.h"

#define SOCKET_NAME "rasdaemon"
#define SERVER_MAX_CONN 10
#define MSG_SIZE 255

struct ras_server {
  pthread_t tid;

  int socketfd;
  struct pollfd *fds;
  int nclients;
};

#ifdef HAVE_BROADCAST

int ras_server_start(void);
void ras_server_stop(void);

int ras_broadcast_mc_event(struct ras_mc_event *ev);
int ras_broadcast_aer_event(struct ras_aer_event *ev);
int ras_broadcast_mce_event(struct mce_event *ev);
int ras_broadcast_non_standard_event(struct ras_non_standard_event *ev);
int ras_broadcast_arm_event(struct ras_arm_event *ev);
int ras_broadcast_devlink_event(struct devlink_event *ev);
int ras_broadcast_diskerror_event(struct diskerror_event *ev);

#else

static inline int ras_server_start(void) { return 0; }
static inline void ras_server_stop(void)  }
static inline int ras_broadcast_mc_event(struct ras_mc_event *ev) { return 0; }
static inline int ras_broadcast_aer_event(struct ras_aer_event *ev) { return 0; }
static inline int ras_broadcast_mce_event(struct mce_event *ev) { return 0; }
static inline int ras_broadcast_non_standard_event(struct ras_non_standard_event *ev) { return 0; }
static inline int ras_broadcast_arm_event(struct ras_arm_event *ev) { return 0; }
static inline int ras_broadcast_devlink_event(struct devlink_event *ev) { return 0; }
static inline int ras_broadcast_diskerror_event(struct diskerror_event *ev) { return 0; }

#endif

#endif