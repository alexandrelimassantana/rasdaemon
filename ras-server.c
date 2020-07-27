#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ras-server.h"
#include "ras-logger.h"

static struct ras_server server;

static void server_destroy(void) {
  if(server.socketfd > 0) {
    shutdown(server.socketfd, SHUT_WR);
    close(server.socketfd);
  }
  if(server.conn)
    free(server.conn);
  memset(&server, 0, sizeof(struct ras_server));
  log(ALL, LOG_INFO, "Broadcasting server has stoped\n");
}

static void server_loop(void) {
  int next_slot, server_full, rv, i;
  struct pollfd *fds;

  // The list of file descriptors to watch
  fds = calloc(server.conn_size+1, sizeof(struct pollfd));

  // Poll the server in the last array slot, ignore the free connection slots
  server_full = 0;
  next_slot = 0;
  fds[server.conn_size].fd = server.socketfd;
  fds[server.conn_size].events = POLLIN;
  for(i = 0; i < server.conn_size; ++i)
    fds[i].fd = -1;

  while(1) {
    rv = poll(fds, server.conn_size+1, -1);
    if(rv < 0) {
      log(ALL, LOG_ERR, "Can't poll the connection file descriptors\n");
      server_destroy();
      exit(1);
    }

    i = 0;
    while(rv > 0) {
      struct pollfd *p = &fds[i];
      // If a connection is closed, release resources
      if(p->revents & POLLHUP) {
        --rv;
        close(p->fd);
        p->fd = -1; // stop tracking in poll()
        server.conn[i] = 0; // logically disable the FD
        next_slot = i; // This slot is free
        // Start monitoring the socket file descriptor once again
        if(server_full) {
          server_full = 0;
          fds[server.conn_size].fd = server.socketfd;
          fds[server.conn_size].events = POLLIN;
        }
        log(ALL, LOG_INFO, "Client %d disconnected from RAS server\n", next_slot);
      }
      // If a connection has opened, set-up context and invoke the handler
      else if(fds[i].revents & POLLIN) {
        --rv;
        // Find the next conn slot if it haven't been found yet
        if(next_slot < 0) {
          for(int w = 0; w < server.conn_size; ++w) {
            if(server.conn[w] == 0) {
              next_slot = w;
              break;
            }
          }
        }
        // No more connection slots, stop polling the server socket
        if(next_slot == -1) {
          fds[server.conn_size].fd = -1;
          server_full = 1;
          continue;
        }
        // Connection succeeded, save file descriptors and watch them in poll()
        server.conn[next_slot] = accept(server.socketfd, NULL, NULL);
        fds[next_slot].fd = server.conn[next_slot];
        fds[next_slot].events = POLLHUP;
        log(ALL, LOG_INFO, "Client %d connected to RAS server\n", next_slot);
        next_slot = -1;
      }
      ++i;
    }
  }
}

int server_begin(void) {
  struct sockaddr_un addr;
  pthread_t tid;

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

  server.conn_size = SERVER_MAX_CONN;
  server.conn = calloc(SERVER_MAX_CONN, sizeof(int));

  if(pthread_create(&tid, NULL, (void*(*)(void*))server_loop, NULL)) {
    log(ALL, LOG_WARNING, "Can't create server thread for managing connections\n");
    goto create_err;
  }

  log(ALL, LOG_INFO, "Server started and listening to connections\n");
  return 0;

  create_err:
  server_destroy();
  return -1;
}

void server_broadcast(const char *msg, size_t size) {
  // TODO: there should be a way to do this better, maybe using splice()
  for(int i = 0; i < server.conn_size; ++i)
    if(server.conn[i] > 0)
      if(write(server.conn[i], msg, size) < 0)
        log(ALL, LOG_ERR, "Can't write to registered client process\n");
}
