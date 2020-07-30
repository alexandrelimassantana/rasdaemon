#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ras-server.h"
#include "ras-logger.h"

static struct ras_server server;

static void server_loop(void) {
  int next_slot, server_full, rv, i;

  // Poll the server in the last array slot, ignore the free connection slots
  server_full = 0;
  next_slot = 0;
  // Set up socket watchers in poll()
  server.fds[server.nclients].fd = server.socketfd;
  server.fds[server.nclients].events = POLLIN;
  // Set up client connection watchers in poll()
  for(int i = 0; i < server.nclients; ++i) {
    server.fds[i].fd = -1;
    server.fds[i].events = POLLHUP;
  }

  while(1) {
    rv = poll(server.fds, server.nclients+1, -1);
    if(rv < 0) {
      log(ALL, LOG_ERR, "Can't poll the connection file descriptors\n");
      ras_server_stop();
    }

    i = 0;
    while(rv > 0) {
      struct pollfd *p = &server.fds[i];
      int clifd = 0;
      // If a connection is closed, release resources
      if(p->revents & POLLHUP) {
        --rv;
        close(p->fd);
        p->fd = -1; // stop tracking in poll()
        next_slot = i; // This slot is free
        // Start monitoring the socket file descriptor once again
        if(server_full) {
          server_full = 0;
          server.fds[server.nclients].fd = server.socketfd;
          server.fds[server.nclients].events = POLLIN;
        }
        log(ALL, LOG_INFO, "Client %d disconnected from RAS server\n", next_slot);
      }
      // If a connection has opened, set-up context and invoke the handler
      else if(p->revents & POLLIN) {
        --rv;
        // Find the next conn slot if it haven't been found yet
        if(next_slot < 0) {
          for(int w = 0; w < server.nclients; ++w) {
            if(server.fds[w].fd < 0) {
              next_slot = w;
              break;
            }
          }
        }
        // No more connection slots, stop polling the server socket
        if(next_slot == -1) {
          server.fds[server.nclients].fd = -1;
          server_full = 1;
          continue;
        }
        // Connection succeeded, save file descriptors and watch them in poll()
        clifd = accept(server.socketfd, NULL, NULL);
        if(clifd > 0 && server.fds != NULL) {
          server.fds[next_slot].fd = clifd;
          log(ALL, LOG_INFO, "Client %d connected to RAS server\n", next_slot);
          next_slot = -1;
        }
      }
      ++i;
    }
  }
}

int ras_server_start(void) {
  struct sockaddr_un addr;

  server.socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(server.socketfd == -1) {
    log(ALL, LOG_WARNING, "Can't create local socket for broadcasting\n");
    goto create_err;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  addr.sun_path[0] = '\0';
  strncpy(addr.sun_path+1, SOCKET_NAME, sizeof(addr.sun_path)-2);
  
  if(bind(server.socketfd, (struct sockaddr*)&addr, sizeof(addr))){
    log(ALL, LOG_WARNING, "Can't bind to local socket for broadcasting\n");
    goto create_err;
  }
  if(listen(server.socketfd, SERVER_MAX_CONN)) {
    log(ALL, LOG_WARNING, "Can't listen on local socket for broadcasting\n");
    goto create_err;
  }

  server.nclients = SERVER_MAX_CONN;
  server.fds = calloc(server.nclients+1, sizeof(struct pollfd));

  if(pthread_create(&server.tid, NULL, (void*(*)(void*))server_loop, NULL)) {
    log(ALL, LOG_WARNING, "Can't create server thread for managing connections\n");
    goto create_err;
  }

  log(ALL, LOG_INFO, "Server started and listening to connections\n");
  signal(SIGPIPE, SIG_IGN);
  return 0;

  create_err:
  ras_server_stop();
  return -1;
}

void ras_server_stop(void) {
  if(server.tid > 0)
    pthread_cancel(server.tid);

  if(server.socketfd > 0) {
    shutdown(server.socketfd, SHUT_WR);
    close(server.socketfd);
  }

  if(server.fds)
    free(server.fds);

  memset(&server, 0, sizeof(struct ras_server));
  signal(SIGPIPE, SIG_DFL);
  log(ALL, LOG_INFO, "RAS server has stoped\n");
}

static void ras_server_broadcast(const char *msg, size_t size) {
  for(int i = 0; i < server.nclients; ++i)
    if(server.fds[i].fd > 0)
      if(write(server.fds[i].fd, msg, size) < 0)
        log(ALL, LOG_ERR, "Can't write to registered client process\n");
}

int ras_broadcast_mc_event(struct ras_mc_event *ev) {
  ras_server_broadcast("type=mc", MSG_SIZE);
  return 0;
}

int ras_broadcast_aer_event(struct ras_aer_event *ev) {
  ras_server_broadcast("type=aer", MSG_SIZE);
  return 0;
}

int ras_broadcast_mce_event(struct mce_event *ev) {
  ras_server_broadcast("type=mce", MSG_SIZE);
  return 0;
}

int ras_broadcast_non_standard_event(struct ras_non_standard_event *ev) {
  ras_server_broadcast("type=non_standard", MSG_SIZE);
  return 0;
}

int ras_broadcast_arm_event(struct ras_arm_event *ev) {
  ras_server_broadcast("type=arm", MSG_SIZE);
  return 0;
}

int ras_broadcast_devlink_event(struct devlink_event *ev) {
  ras_server_broadcast("type=devlink", MSG_SIZE);
  return 0;
}

int ras_broadcast_diskerror_event(struct diskerror_event *ev) {
  ras_server_broadcast("type=diskerror", MSG_SIZE);
  return 0;
}
