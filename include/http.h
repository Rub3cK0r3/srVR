/*
 * Minimal HTTP request representation and parsing utilities.
 *
 * The goal is educational rather than fully compliant: we
 * focus on the request line, a handful of headers, and an
 * optional message body for POST requests.
 */

#ifndef SRVR_HTTP_H
#define SRVR_HTTP_H

#include <stddef.h>

#define HTTP_MAX_METHOD_LEN 8
#define HTTP_MAX_PATH_LEN 512
#define HTTP_MAX_VERSION_LEN 16
#define HTTP_MAX_HEADERS 32

typedef struct http_header {
  char name[64];
  char value[256];
} http_header;

typedef struct http_request {
  char method[HTTP_MAX_METHOD_LEN];
  char path[HTTP_MAX_PATH_LEN];
  char version[HTTP_MAX_VERSION_LEN];

  http_header headers[HTTP_MAX_HEADERS];
  size_t header_count;

  char *body;
  size_t body_length;
} http_request;

/*
 * Parse a raw HTTP request buffer into an http_request structure.
 *
 * HTTP requests start with a single "request line" followed by
 * header lines and an empty line, for example:
 *
 *   GET /index.html HTTP/1.1\r\n
 *   Host: example.com\r\n
 *   User-Agent: curl/8.0\r\n
 *   \r\n
 *
 * Returns 0 on success, -1 on parse error.
 */
int http_parse_request(char *buffer, size_t length, http_request *req);

/*
 * Convenience helper to look up a header by name. HTTP header
 * field names are case-insensitive, so we compare them in a
 * case‑insensitive way here.
 *
 * Returns a pointer to the header value string, or NULL if not found.
 */
const char *http_get_header(const http_request *req, const char *name);

/*
 * Deduce a MIME type from a file name or path by looking at
 * the file extension. Web servers send this in the
 * "Content-Type" header so that browsers know how to interpret
 * the payload.
 */
const char *get_mime_type(const char *path);

#endif /* SRVR_HTTP_H */

