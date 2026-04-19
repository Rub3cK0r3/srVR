/*
* main.c
*
* Copyright (c) 2026 Rub3ck0r3
* Licensed under the MIT License.
*/

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

/*
* Global running flag used for graceful shutdown.
* The SIGINT handler simply flips this flag; the main
* accept loop checks it and exits cleanly.
*/
volatile sig_atomic_t srvr_running = 1;

static void handle_sigint(int sig) {
  (void)sig;
  srvr_running = 0;
}

int main(void) {
  server_config cfg;
  if (load_server_config("config/server.conf", &cfg) == -1) {
    log_warn("Failed to load config file, using defaults");
  }

  /* Install a SIGINT handler using sigaction(2).
   *
   * Historically, signal(2) had implementation-defined semantics:
   * handlers could be reset after delivery and there were races
   * around interrupted system calls. sigaction(2) gives us
   * well-defined, POSIX behaviour and lets us control flags such
   * as whether system calls are automatically restarted.
   */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  int serverfd = server_listen(&cfg);
  if (serverfd == -1) {
    log_error("Failed to create listening socket");
    return EXIT_FAILURE;
  }

  log_info("Listening on port %d, root=%s", cfg.port, cfg.document_root);

  if (cfg.use_epoll) {
    server_run_epoll(serverfd, &cfg);
  } else {
    server_run_threaded(serverfd, &cfg);
  }

  return EXIT_SUCCESS;
}
