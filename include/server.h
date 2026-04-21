/*
 * Core server interfaces and configuration.
 *
 * This header intentionally keeps the public surface small:
 * - configuration loading
 * - server lifecycle
 * - logging helpers
 */

#ifndef SRVR_SERVER_H
#define SRVR_SERVER_H

#include <netinet/in.h>
#include <stddef.h>
#include <signal.h>
#include <time.h>

/* Default values used when no configuration file is present. */
#define SRVR_DEFAULT_PORT 8080
#define SRVR_DEFAULT_ROOT "./www"

/*
 * Global server configuration loaded from server.conf.
 *
 * For this project we keep it simple: just the TCP port and the
 * document root used by the static file server.
 */
typedef struct server_config {
  int port;
  char document_root[512];
  int use_epoll; /* non‑zero when the optional epoll loop is enabled */
} server_config;

/* Example server config file - server.conf ..: 
* port=8080 
* document_root=./www
* use_epoll=0
*/

//
//

/*
 * Initialize configuration with sane defaults, then optionally
 * override them from a simple key=value configuration file.
 *
 * Returns 0 on success, -1 on parse error.
 */
int load_server_config(const char *path, server_config *cfg);

/*
 * Create, bind and listen on a TCP socket according to the
 * provided configuration. The returned descriptor is ready
 * to be used with accept(2) or an event loop.
 *
 * Returns a non‑negative file descriptor on success, -1 on error.
 */
int server_listen(const server_config *cfg);

/*
 * Blocking accept loop that creates a worker thread per connection.
 * This keeps the concurrency model simple while still allowing
 * multiple clients to be served in parallel.
 */
void server_run_threaded(int serverfd, const server_config *cfg);

/*
 * Optional epoll‑based event loop. When enabled from configuration,
 * the server uses non‑blocking sockets and an event‑driven design
 * instead of one thread per connection.
 */
void server_run_epoll(int serverfd, const server_config *cfg);

/*
 * Graceful shutdown flag manipulated by the SIGINT handler.
 * The main loop checks this flag to stop accepting new clients
 * and close the listening socket.
 */
extern volatile sig_atomic_t srvr_running;

/*
 * Simple logging helpers. All log entries are timestamped and
 * appended to server.log; they can also be echoed to stderr
 * for interactive debugging.
 */
void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif /* SRVR_SERVER_H */

