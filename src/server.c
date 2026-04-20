/*
* server.c
*
* Copyright (c) 2026 Rub3ck0r3
* Licensed under the MIT License.
*/

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

/**
* @brief Global flag indicating whether the server should continue running.
*
* This variable is typically modified by signal handlers (e.g., SIGINT)
* to gracefully stop the server loop.
*
* @note Declared as volatile sig_atomic_t to ensure safe access in
*       asynchronous signal contexts.
*/
extern volatile sig_atomic_t srvr_running;

static const char *LOG_PATH = "server.log";

/**
* @brief Write a formatted log message with a severity level.
*
* Opens the log file, prepends a timestamp and severity level, and writes
* the formatted message. Each log entry is written on a new line.
*
* @param level String representing the log level (e.g., "INFO", "ERROR").
* @param fmt printf-style format string.
* @param ap Variable argument list corresponding to the format string.
*
* @note The log file is opened and closed on each call.
* @note If the file cannot be opened, the function silently returns.
* @note Not safe for use in signal handlers due to use of stdio and time functions.
*/
static void log_with_level(const char *level, const char *fmt, va_list ap) {
  FILE *fp = fopen(LOG_PATH, "a");
  if (!fp) {
    return;
  }

  // We get the localtime_r right now to be able to print it in 
  // logs
  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);

  // Creation of a temporal buffer where i store custom log data
  char tbuf[32];
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_now);

  // Now i place the log on the file
  fprintf(fp, "[%s] %s: ", tbuf, level);
  vfprintf(fp, fmt, ap);
  fputc('\n', fp);

  // Finally close the file...
  fclose(fp);
}

/**
* @brief Log an informational message.
*
* Formats and writes a message with the "INFO" level to the log file.
*
* @param fmt printf-style format string.
* @param ... Additional arguments corresponding to the format string.
*/

void log_info(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_with_level("INFO", fmt, ap);
  va_end(ap);
}

/**
* @brief Log a warning message.
*
* Formats and writes a message with the "WARN" level to the log file.
*
* @param fmt printf-style format string.
* @param ... Additional arguments corresponding to the format string.
*/
void log_warn(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_with_level("WARN", fmt, ap);
  va_end(ap);
}

/**
* @brief Log an error message.
*
* Formats and writes a message with the "ERROR" level to the log file.
*
* @param fmt printf-style format string.
* @param ... Additional arguments corresponding to the format string.
*/
void log_error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_with_level("ERROR", fmt, ap);
  va_end(ap);
}

/**
* @brief Load server configuration from a file.
*
* Reads a configuration file containing simple key=value pairs and
* populates the provided server_config structure. If the file cannot
* be opened, default values are used and the function still succeeds.
*
* Supported configuration keys:
*   - port        : Server listening port (integer)
*   - root        : Document root directory (string)
*   - use_epoll   : Enable epoll (0 or non-zero)
*
* Lines beginning with '#' or blank lines are ignored.
*
* @param path Path to the configuration file.
* @param cfg Pointer to a server_config structure to populate.
*
* @return 0 on success (including when file is missing).
*
* @note The configuration structure is initialized with default values
*       before parsing the file.
* @note Parsing is simple and does not handle whitespace trimming or
*       malformed input robustly.
*/
int load_server_config(const char *path, server_config *cfg) {
  // Load server_config from included data config file.. (include)
  cfg->port = SRVR_DEFAULT_PORT;
  strncpy(cfg->document_root, SRVR_DEFAULT_ROOT, sizeof(cfg->document_root));
  cfg->document_root[sizeof(cfg->document_root) - 1] = '\0';
  cfg->use_epoll = 0; // For now we are going to be testing the server_run_threaded version
  //
  //

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

/**
* @brief Set up a listening TCP socket for the server.
*
* Creates an IPv4 TCP socket, enables address reuse, binds it to
* INADDR_ANY and the configured port, and begins listening for
* incoming connections.
*
* @param cfg Pointer to server configuration.
*
* @return Socket file descriptor on success, -1 on error.
*
* @note On failure, an error message is printed using perror().
* @note The socket listens on all available network interfaces.
* @note The backlog is set to SOMAXCONN for maximum queue capacity.
*/
int server_listen(const server_config *cfg) {
  int serverfd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverfd == -1) {
    perror("socket");
    return -1;
  }

  int opt = 1;
  // The setsockopt() function provides an application program with the
  // means to control socket behavior. An application program can use
  // setsockopt() to allocate buffer space, control timeouts, or permit
  // socket data broadcasts. The <sys/socket.h> header defines the
  // socket-level options available to setsockopt().
  //
  // Options may exist at multiple protocol levels. The SO_ options are
  // always present at the uppermost socket level.
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

/**
* @brief Arguments passed to a client handler thread.
*
* Contains the client socket file descriptor and a copy of the
* server configuration needed to handle the connection.
*/
typedef struct client_thread_args {
  int clientfd;
  server_config cfg;
} client_thread_args;

/**
* @brief Entry point for handling a client connection in a separate thread.
*
* This function is intended to be used as the start routine for a thread.
* It processes a single client connection, logs the event, invokes the
* request handler, and performs cleanup.
*
* @param arg Pointer to a dynamically allocated client_thread_args structure.
*
* @return NULL on completion.
*
* @note The function takes ownership of the argument pointer and frees it.
* @note The client socket is closed before the thread exits.
* @note Not safe to reuse the argument after passing it to this function.
*/
static void *client_thread_main(void *arg) {
  client_thread_args *cta = (client_thread_args *)arg;
  log_info("Accepted client fd=%d", cta->clientfd);
  handle_client_connection(cta->clientfd, &cta->cfg);
  close(cta->clientfd);
  // REMEMBER:
  // The free() function shall cause the space pointed to by ptr to be
  // deallocated; that is, made available for further allocation. If
  // ptr is a null pointer, no action shall occur. Otherwise, if the
  // argument does not match a pointer earlier returned by a function
  // in POSIX.1‐2008 that allocates memory as if by malloc(), or if the
  // space has been deallocated by a call to free() or realloc(), the
  // behavior is undefined.
  //
  // Any use of a pointer that refers to freed space results in
  // undefined behavior.
  free(cta);
  return NULL;
}

/**
* @brief Run the server using a thread-per-connection model.
*
* Continuously accepts incoming client connections on the given
* listening socket and spawns a new detached thread to handle
* each connection.
*
* @param serverfd Listening socket file descriptor.
* @param cfg Pointer to server configuration.
*
* @note Each client connection is handled in its own thread.
* @note Threads are detached and clean up their own resources.
* @note The server loop runs until the global srvr_running flag is cleared.
* @note The listening socket is closed before the function returns.
*/
void server_run_threaded(int serverfd, const server_config *cfg) {
  struct sockaddr_in clientaddr;
  socklen_t client_len = sizeof(clientaddr);

  // REMEMBER (srvr_running):
  // This directs the compiler not to do certain common optimizations 
  // on use of the variable lock. All the reads and writes for a volatile 
  // variable or field are really done, and done in the order specified by 
  // the source code. 
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

    // The pthread_create() function starts a new thread in the calling
    // process.The new thread starts execution by invoking
    // start_routine(); arg is passed as the sole argument of
    // start_routine().
    pthread_t tid;
    if (pthread_create(&tid, NULL, client_thread_main, cta) != 0) {
      log_error("Failed to create client thread");
      close(clientfd);
      free(cta);
      continue;
    }
    // The pthread_detach() function marks the thread identified by
    // thread as detached.  When a detached thread terminates
    pthread_detach(tid);
  }

  close(serverfd);
}

/**
* @brief Set a file descriptor to non-blocking mode.
*
* Retrieves the current file status flags for the given file descriptor
* and adds the O_NONBLOCK flag, enabling non-blocking I/O operations.
*
* @param fd File descriptor to modify.
*
* @return 0 on success, -1 on failure.
*
* @note On failure, errno is set by fcntl().
* @note This function preserves existing file status flags.
*/
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

/** TODO: This implementation is demo yet.
* @brief Event-driven server loop using epoll.
*
* Creates an epoll instance and registers the listening socket to
* receive incoming connection events. Accepts new clients in a
* non-blocking loop and registers them with epoll for I/O readiness
* notifications.
*
* Remember: An event can be defined as "a significant change in state". 
* For example, when a consumer purchases a car, the car's state changes 
* from "for sale" to "sold". 
* A car dealer's system architecture may treat this state change as an 
* event whose occurrence can be made known to other applications within 
* the architecture.
*
* Client sockets are handled sequentially once they become readable
* and are then closed.
*
* @param serverfd Listening socket descriptor.
* @param cfg Pointer to server configuration.
*
* @note Non-blocking sockets are required for correct epoll behavior.
* @note Uses EPOLLET (edge-triggered mode) for client sockets.
* @note Designed as a simplified educational event loop, not a
*       fully stateful production HTTP engine.
*/
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

            // 🔹 NEW CONNECTIONS
            if (events[i].data.fd == serverfd) {
                while (1) {
                    struct sockaddr_in clientaddr;
                    socklen_t len = sizeof(clientaddr);

                    int clientfd = accept(serverfd, (struct sockaddr *)&clientaddr, &len);
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

                    epoll_client *client = malloc(sizeof(epoll_client));
                    if (!client) {
                        perror("malloc");
                        close(clientfd);
                        continue;
                    }

                    client->clientfd = clientfd;
                    client->buffer_len = 0;

                    struct epoll_event cev;
                    cev.events = EPOLLIN | EPOLLET;
                    cev.data.ptr = client;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &cev) == -1) {
                        perror("epoll_ctl ADD client");
                        close(clientfd);
                        free(client);
                        continue;
                    }
                }
            }

            // 🔹 EXISTING CLIENTS
            else {
                epoll_client *client = (epoll_client *)events[i].data.ptr;
                int clientfd = client->clientfd;

                // handle disconnects/errors
                if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    close(clientfd);
                    free(client);
                    continue;
                }

                // handle readable data
                if (events[i].events & EPOLLIN) {
                    while (1) {
                        ssize_t bytes = read(clientfd, client->buff, sizeof(client->buff));

                        if (bytes == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            }
                            perror("read");
                            close(clientfd);
                            free(client);
                            break;
                        } else if (bytes == 0) {
                            close(clientfd);
                            free(client);
                            break;
                        } else {
                            client->buffer_len = bytes;
                            printf("Received: %.*s\n", (int)bytes, client->buff);
                        }
                    }
                }
            }
        }
    }

    close(epfd);
    close(serverfd);
}

