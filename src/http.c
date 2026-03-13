#define _GNU_SOURCE

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"

static void trim(char *s) {
  char *end;
  while (*s && isspace((unsigned char)*s)) {
    s++;
  }
  if (*s == 0) {
    return;
  }
  end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) {
    *end-- = '\0';
  }
}

static int ci_equal(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return 0;
    }
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

int http_parse_request(char *buffer, size_t length, http_request *req) {
  memset(req, 0, sizeof(*req));

  /*
   * HTTP requests start with a single line:
   *   <METHOD> <PATH> <VERSION>\r\n
   * followed by header lines "Name: Value\r\n" and a blank line.
   */

  char *pos = buffer;
  char *end = buffer + length;
  char *line_end = strstr(pos, "\r\n");
  if (!line_end) {
    return -1;
  }
  *line_end = '\0';

  if (sscanf(pos, "%7s %511s %15s", req->method, req->path, req->version) !=
      3) {
    return -1;
  }

  pos = line_end + 2;

  /* Parse headers until we hit an empty line. */
  while (pos < end && req->header_count < HTTP_MAX_HEADERS) {
    line_end = strstr(pos, "\r\n");
    if (!line_end) {
      break;
    }
    if (line_end == pos) {
      /* Empty line: end of headers. */
      pos += 2;
      break;
    }

    *line_end = '\0';
    char *colon = strchr(pos, ':');
    if (colon) {
      *colon = '\0';
      char *name = pos;
      char *value = colon + 1;
      trim(name);
      trim(value);

      http_header *h = &req->headers[req->header_count++];
      strncpy(h->name, name, sizeof(h->name));
      h->name[sizeof(h->name) - 1] = '\0';
      strncpy(h->value, value, sizeof(h->value));
      h->value[sizeof(h->value) - 1] = '\0';
    }

    pos = line_end + 2;
  }

  /* The caller is responsible for reading any body bytes based
   * on the Content-Length header; we just record the pointer. */
  if (pos < end) {
    req->body = pos;
    req->body_length = (size_t)(end - pos);
  }

  return 0;
}

const char *http_get_header(const http_request *req, const char *name) {
  for (size_t i = 0; i < req->header_count; ++i) {
    if (ci_equal(req->headers[i].name, name)) {
      return req->headers[i].value;
    }
  }
  return NULL;
}

const char *get_mime_type(const char *path) {
  /*
   * MIME types (media types) tell the client what kind of content
   * is being sent: HTML, CSS, images, plain text, etc. Browsers use
   * this to decide how to render or handle the response body.
   */
  const char *ext = strrchr(path, '.');
  if (!ext) {
    return "application/octet-stream";
  }
  ext++;

  if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) {
    return "text/html; charset=utf-8";
  } else if (strcmp(ext, "css") == 0) {
    return "text/css; charset=utf-8";
  } else if (strcmp(ext, "js") == 0) {
    return "application/javascript";
  } else if (strcmp(ext, "png") == 0) {
    return "image/png";
  } else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
    return "image/jpeg";
  } else if (strcmp(ext, "txt") == 0) {
    return "text/plain; charset=utf-8";
  }

  return "application/octet-stream";
}

