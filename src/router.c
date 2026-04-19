/*
* router.c
*
* Copyright (c) 2026 Rub3ck0r3
* Licensed under the MIT License.
*/

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "http.h"
#include "router.h"
#include "server.h"

/**
* @brief Read available data from a socket with HTTP-aware early exit.
*
* Reads from a TCP socket in a loop until one of the following occurs:
* - Maximum buffer size is reached
* - Connection is closed
* - HTTP header terminator "\r\n\r\n" is found
*
* Interrupted system calls are automatically retried.
*
* @param fd Socket descriptor.
* @param buf Buffer to store received data.
* @param max_len Maximum buffer capacity.
*
* @return Total bytes read, 0 on connection close, -1 on error.
*
* @note Designed for simple HTTP request ingestion, not general-purpose
*       streaming or binary protocols.
*/
static ssize_t recv_all(int fd, char *buf, size_t max_len) {
  size_t total = 0;
  while (total < max_len) {
    ssize_t n = recv(fd, buf + total, max_len - total, 0);
    if (n == -1) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
    /* For HTTP we can stop early if we have read the header terminator. */
    if (total >= 4 &&
        strstr(buf, "\r\n\r\n") != NULL) {
      break;
    }
  }
  return (ssize_t)total;
}

/**
* @brief Send all bytes in a buffer over a socket.
*
* Repeatedly calls send() until all data in the buffer has been
* transmitted or an unrecoverable error occurs. Handles interrupted
* system calls and non-blocking socket conditions transparently.
*
* @param fd Socket file descriptor to send data through.
* @param buf Pointer to the data buffer to send.
* @param len Number of bytes to send.
*
* @return Number of bytes successfully sent on success,
*         -1 on error.
*
* @note This function may loop indefinitely on EAGAIN/EWOULDBLOCK
*       if the socket never becomes writable again.
* @note Intended for TCP sockets; not suitable for datagram sockets.
*/
static ssize_t send_all(int fd, const void *buf, size_t len) {
  size_t total = 0;
  const char *p = (const char *)buf;
  while (total < len) {
    ssize_t n = send(fd, p + total, len - total, 0);
    if (n == -1) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      return -1;
    }
    total += (size_t)n;
  }
  return (ssize_t)total;
}

static int contains_dotdot(const char *path) {
  /*
   * Directory traversal attacks try to escape the configured
   * document root by using ".." segments in the URL, e.g.
   * "/../../etc/passwd". We defensively reject any path that
   * contains ".." to ensure clients cannot read arbitrary files
   * from the filesystem.
   */
  return strstr(path, "..") != NULL;
}

/**
* @brief Send a simple HTTP response with optional body.
*
* Constructs and sends a minimal HTTP/1.1 response containing a status
* line, basic headers, and an optional response body. The connection
* is always closed after the response.
*
* @param fd Socket file descriptor to send the response to.
* @param status HTTP status code (e.g., 200, 404).
* @param reason Human-readable reason phrase (e.g., "OK", "Not Found").
* @param body Optional response body (may be NULL).
* @param content_type MIME type of the response body. If NULL,
*        defaults to "text/plain; charset=utf-8".
*
* @note The function uses Content-Length based on strlen(body).
* @note Response headers are written into a fixed-size buffer (512 bytes).
* @note If header formatting exceeds the buffer, the response is dropped.
* @note Uses send_all(), which may block depending on socket state.
*/
static void send_simple_response(int fd, int status,
                                 const char *reason,
                                 const char *body,
                                 const char *content_type) {
  char header[512];
  size_t body_len = body ? strlen(body) : 0;
  int n = snprintf(header, sizeof(header),
                   "HTTP/1.1 %d %s\r\n"
                   "Content-Type: %s\r\n"
                   "Content-Length: %zu\r\n"
                   "Connection: close\r\n"
                   "\r\n",
                   status, reason,
                   content_type ? content_type : "text/plain; charset=utf-8",
                   body_len);
  if (n < 0 || (size_t)n >= sizeof(header)) {
    return;
  }
  send_all(fd, header, (size_t)n);
  if (body_len > 0) {
    send_all(fd, body, body_len);
  }
}

/**
* @brief Serve a static file from the document root.
*
* Maps the HTTP request path to a file under the configured document root,
* validates the path for security, and streams the file using sendfile().
* If the file does not exist or is invalid, an HTTP error response is sent.
*
* @param fd Client socket descriptor.
* @param cfg Server configuration containing document_root.
* @param path Requested URL path.
*
* @note "/" is mapped to "/index.html".
* @note Basic directory traversal protection is applied via contains_dotdot().
* @note Uses stat() to validate file existence and type.
* @note Uses sendfile() for zero-copy file transmission when possible.
*/
static void serve_static_file(int fd, const server_config *cfg,
                              const char *path) {
  char resolved[1024];
  const char *rel = path;
  if (strcmp(path, "/") == 0) {
    rel = "/index.html";
  }

  if (contains_dotdot(rel)) {
    send_simple_response(fd, 403, "Forbidden",
                         "403 Forbidden\r\n",
                         "text/plain; charset=utf-8");
    return;
  }

  snprintf(resolved, sizeof(resolved), "%s%s", cfg->document_root, rel);

  struct stat st;
  if (stat(resolved, &st) == -1 || !S_ISREG(st.st_mode)) {
    send_simple_response(fd, 404, "Not Found",
                         "404 Not Found\r\n",
                         "text/plain; charset=utf-8");
    return;
  }

  int filefd = open(resolved, O_RDONLY);
  if (filefd == -1) {
    send_simple_response(fd, 500, "Internal Server Error",
                         "500 Internal Server Error\r\n",
                         "text/plain; charset=utf-8");
    return;
  }

  const char *mime = get_mime_type(resolved);
  char header[512];
  int n = snprintf(header, sizeof(header),
                   "HTTP/1.1 200 OK\r\n"
                   "Content-Type: %s\r\n"
                   "Content-Length: %zu\r\n"
                   "Connection: close\r\n"
                   "\r\n",
                   mime, (size_t)st.st_size);
  if (n < 0 || (size_t)n >= sizeof(header)) {
    close(filefd);
    return;
  }

  if (send_all(fd, header, (size_t)n) == -1) {
    close(filefd);
    return;
  }

  off_t offset = 0;
  ssize_t sent;
  while (offset < st.st_size &&
         (sent = sendfile(fd, filefd, &offset, (size_t)(st.st_size - offset))) >
             0) {
  }

  close(filefd);
}

/**
* @brief Handle a single HTTP client connection.
*
* Reads and parses an HTTP request from the client socket, logs the
* request, optionally processes a request body (for POST requests),
* and dispatches the request to the appropriate handler.
*
* Supported methods:
*   - GET    : Serves static files
*   - HEAD   : Served as static file response (same as GET here)
*   - POST   : Serves static file and prints request body to stdout
*
* Unsupported methods receive HTTP 405 Method Not Allowed.
*
* @param clientfd Client socket file descriptor.
* @param cfg Pointer to server configuration.
*
* @note This function currently uses a fixed-size stack buffer (8KB).
* @note Request parsing is limited to a single recv_all() call plus
*       optional extra body read for POST requests.
* @note POST body handling is demonstration-only and not production-safe.
*/
void handle_client_connection(int clientfd, const server_config *cfg) {
  char buffer[8192];
  ssize_t n = recv_all(clientfd, buffer, sizeof(buffer) - 1);
  if (n <= 0) {
    return;
  }
  buffer[n] = '\0';

  http_request req;
  if (http_parse_request(buffer, (size_t)n, &req) == -1) {
    send_simple_response(clientfd, 400, "Bad Request",
                         "400 Bad Request\r\n",
                         "text/plain; charset=utf-8");
    return;
  }

  log_info("Request: %s %s %s", req.method, req.path, req.version);

  const char *cl = http_get_header(&req, "Content-Length");
  if (cl && strcmp(req.method, "POST") == 0) {
    long len = strtol(cl, NULL, 10);
    if (len > 0 && (size_t)len > req.body_length &&
        (size_t)len < sizeof(buffer)) {
      ssize_t extra =
          recv_all(clientfd, buffer + n, (size_t)len - req.body_length);
      if (extra > 0) {
        req.body = buffer + (n - req.body_length);
        req.body_length += (size_t)extra;
      }
    }

    /*
     * For demonstration we simply print the POST body to the
     * console. In a real application this is where you would
     * parse form data or JSON payloads.
     */
    if (req.body && req.body_length > 0) {
      fwrite(req.body, 1, req.body_length, stdout);
      fputc('\n', stdout);
    }
  }

  if (strcmp(req.method, "GET") == 0 || strcmp(req.method, "HEAD") == 0) {
    serve_static_file(clientfd, cfg, req.path);
  } else if (strcmp(req.method, "POST") == 0) {
    serve_static_file(clientfd, cfg, req.path);
  } else {
    send_simple_response(clientfd, 405, "Method Not Allowed",
                         "405 Method Not Allowed\r\n",
                         "text/plain; charset=utf-8");
  }
}
