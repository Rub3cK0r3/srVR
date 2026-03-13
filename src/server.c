#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "router.h"
#include "server.h"

extern volatile sig_atomic_t srvr_running;

static const char *LOG_PATH = "server.log";

static void log_with_level(const char *level, const char *fmt, va_list ap) {
  FILE *fp = fopen(LOG_PATH, "a");
  if (!fp) {
    return;
  }

  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);

  char tbuf[32];
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_now);

  fprintf(fp, "[%s] %s: ", tbuf, level);
  vfprintf(fp, fmt, ap);
  fputc('\n', fp);

  fclose(fp);
}

void log_info(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_with_level("INFO", fmt, ap);
  va_end(ap);
}

void log_warn(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_with_level("WARN", fmt, ap);
  va_end(ap);
}

void log_error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_with_level("ERROR", fmt, ap);
  va_end(ap);
}

int load_server_config(const char *path, server_config *cfg) {
  cfg->port = SRVR_DEFAULT_PORT;
  strncpy(cfg->document_root, SRVR_DEFAULT_ROOT, sizeof(cfg->document_root));
  cfg->document_root[sizeof(cfg->document_root) - 1] = '\0';
  cfg->use_epoll = 0;

  FILE *fp = fopen(path, "r");
  if (!fp) {
    /* No config file is not fatal: we keep defaults. */
    return 0;
  }

  char line[512];
  while (fgets(line, sizeof(line), fp)) {
    /* Very simple key=value parsing, skipping comments and blank lines. */
    if (line[0] == '#' || line[0] == '\n') {
      continue;
    }
    char *eq = strchr(line, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    char *key = line;
    char *value = eq + 1;

    /* Strip trailing newline from the value. */
    char *nl = strchr(value, '\n');
    if (nl) {
      *nl = '\0';
    }

    if (strcmp(key, "port") == 0) {
      cfg->port = atoi(value);
    } else if (strcmp(key, "root") == 0) {
      strncpy(cfg->document_root, value, sizeof(cfg->document_root));
      cfg->document_root[sizeof(cfg->document_root) - 1] = '\0';
    } else if (strcmp(key, "use_epoll") == 0) {
      cfg->use_epoll = atoi(value) != 0;
    }
  }

  fclose(fp);
  return 0;
}

int server_listen(const server_config *cfg) {
  int serverfd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverfd == -1) {
    perror("socket");
    return -1;
  }

  int opt = 1;
  setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(cfg->port);

  if (bind(serverfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    close(serverfd);
    return -1;
  }

  /*
   * The second argument to listen() is the "backlog": the maximum number
   * of pending connections that can wait in the queue before accept()
   * picks them up. Using 1 here, as educational examples sometimes do,
   * would mean only a single connection can wait; any burst of clients
   * would quickly see connection failures. SOMAXCONN asks the kernel to
   * use its maximum reasonable queue length for a production-style
   * server.
   */
  if (listen(serverfd, SOMAXCONN) == -1) {
    perror("listen");
    close(serverfd);
    return -1;
  }

  return serverfd;
}

typedef struct client_thread_args {
  int clientfd;
  server_config cfg;
} client_thread_args;

static void *client_thread_main(void *arg) {
  client_thread_args *cta = (client_thread_args *)arg;
  log_info("Accepted client fd=%d", cta->clientfd);
  handle_client_connection(cta->clientfd, &cta->cfg);
  close(cta->clientfd);
  free(cta);
  return NULL;
}

void server_run_threaded(int serverfd, const server_config *cfg) {
  struct sockaddr_in clientaddr;
  socklen_t client_len = sizeof(clientaddr);

  while (srvr_running) {
    int clientfd =
        accept(serverfd, (struct sockaddr *)&clientaddr, &client_len);
    if (clientfd == -1) {
      if (!srvr_running && (errno == EINTR || errno == EBADF)) {
        break;
      }
      perror("accept");
      continue;
    }

    client_thread_args *cta = malloc(sizeof(*cta));
    if (!cta) {
      log_error("Out of memory for client_thread_args");
      close(clientfd);
      continue;
    }
    cta->clientfd = clientfd;
    cta->cfg = *cfg;

    pthread_t tid;
    if (pthread_create(&tid, NULL, client_thread_main, cta) != 0) {
      log_error("Failed to create client thread");
      close(clientfd);
      free(cta);
      continue;
    }

    pthread_detach(tid);
  }

  close(serverfd);
}

static int make_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return -1;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return -1;
  }
  return 0;
}

void server_run_epoll(int serverfd, const server_config *cfg) {
  if (make_nonblocking(serverfd) == -1) {
    perror("fcntl");
    close(serverfd);
    return;
  }

  int epfd = epoll_create1(0);
  if (epfd == -1) {
    perror("epoll_create1");
    close(serverfd);
    return;
  }

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = serverfd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, serverfd, &ev) == -1) {
    perror("epoll_ctl");
    close(epfd);
    close(serverfd);
    return;
  }

  /*
   * In an event‑driven architecture we do not block in accept()
   * or recv(); instead we ask the kernel to tell us when sockets
   * become readable or writable. epoll_wait() returns a batch of
   * ready file descriptors which we then service in a loop.
   */
  const int MAX_EVENTS = 64;
  struct epoll_event events[MAX_EVENTS];

  while (srvr_running) {
    int n = epoll_wait(epfd, events, MAX_EVENTS, 1000);
    if (n == -1) {
      if (errno == EINTR) {
        continue;
      }
      perror("epoll_wait");
      break;
    }

    for (int i = 0; i < n; ++i) {
      if (events[i].data.fd == serverfd) {
        /* New incoming connections. */
        while (1) {
          struct sockaddr_in clientaddr;
          socklen_t len = sizeof(clientaddr);
          int clientfd =
              accept(serverfd, (struct sockaddr *)&clientaddr, &len);
          if (clientfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            perror("accept");
            break;
          }

          if (make_nonblocking(clientfd) == -1) {
            close(clientfd);
            continue;
          }

          struct epoll_event cev;
          cev.events = EPOLLIN | EPOLLET;
          cev.data.fd = clientfd;
          if (epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &cev) == -1) {
            perror("epoll_ctl ADD client");
            close(clientfd);
            continue;
          }
        }
      } else {
        int clientfd = events[i].data.fd;
        /*
         * For simplicity we handle each ready client in a
         * blocking fashion here and then close it. In a more
         * advanced design we would keep per‑connection state
         * and perform incremental non‑blocking I/O.
         */
        handle_client_connection(clientfd, cfg);
        epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, NULL);
        close(clientfd);
      }
    }
  }

  close(epfd);
  close(serverfd);
}

