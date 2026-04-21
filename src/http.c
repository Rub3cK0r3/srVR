/*
* http.c
*
* Copyright (c) 2026 Rub3ck0r3
* Licensed under the MIT License.
*/

#define _GNU_SOURCE

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"

/**
* @brief Trims leading and trailing whitespace from a string.
*
* Removes all whitespace characters from the beginning and end of
* the given null-terminated string. Trailing whitespace is removed
* in place by inserting null terminators. Leading whitespace is
* skipped by advancing the pointer.
*
* @param s Pointer to the null-terminated string to be trimmed.
*
* @note This function does not modify the original pointer outside
*       its scope. Leading whitespace is not removed from the original
*       buffer unless the returned pointer is used.
*/
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

/**
* @brief Compare two strings for case-insensitive equality.
*
* Compares two null-terminated strings character by character,
* ignoring differences in letter case.
*
* @param a Pointer to the first string.
* @param b Pointer to the second string.
*
* @return 1 if both strings are equal ignoring case, 0 otherwise.
*/
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

/**
* @brief Parse a raw HTTP request buffer into a structured request object.
*
* Parses an HTTP request from the given buffer, extracting the request line
* (method, path, version) and headers. Header names and values are trimmed
* of surrounding whitespace and stored in the provided request structure.
*
* The function modifies the input buffer in place by inserting null
* terminators ('\0') to split lines and tokens.
*
* @param buffer Pointer to the raw HTTP request data.
* @param length Length of the buffer in bytes.
* @param req Pointer to an http_request structure to populate.
*
* @return 0 on success, -1 on parse error (e.g., malformed request line).
*
* @note The buffer must be mutable since it is modified during parsing.
* @note The request body is not parsed; only a pointer and length are set.
* @note Header parsing stops when an empty line is encountered or when
*       HTTP_MAX_HEADERS is reached.
*/
int http_parse_request(char *buffer, size_t length, http_request *req) {
  // The memset() function fills the first n bytes of the memory area
  // pointed to by s with the constant byte c.
  memset(req, 0, sizeof(*req));

  /*
   * HTTP requests start with a single line:
   *   <METHOD> <PATH> <VERSION>\r\n
   * followed by header lines "Name: Value\r\n" and a blank line.
   */

  char *pos = buffer;
  char *end = buffer + length;
  // The strstr() function finds the first occurrence of the substring
  // needle in the string haystack.  The terminating null bytes ('\0')
  // are not compared.
  // Remember, '\r\n' is 2 characters long (2 bytes)
  char *line_end = strstr(pos, "\r\n");
  if (!line_end) {
    return -1;
  }
  *line_end = '\0';
  // The sscanf() family of functions scans formatted input according
  // to format as described below.
  // This format may contain conversion
  // specifications; the results from such conversions, if any, are
  // stored in the locations pointed to by the pointer arguments that
  // follow format.  Each pointer argument must be of a type that is
  // appropriate for the value returned by the corresponding conversion
  // specification.
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
    // The strchr() function returns a pointer to the first occurrence of
    // the character c in the string s.
    char *colon = strchr(pos, ':');
    if (colon) {
      *colon = '\0';
      char *name = pos;
      char *value = colon + 1;
      trim(name);
      trim(value);

      http_header *h = &req->headers[req->header_count++];
      // stpcpy() - strcpy()
      // These functions copy the string pointed to by src, into a
      // string at the buffer pointed to by dst.  The programmer is
      // responsible for allocating a destination buffer large
      // enough, that is, strlen(src) + 1.  For the difference
      // between the two functions, see RETURN VALUE.
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

/**
* @brief Retrieve the value of a header from an HTTP request.
*
* Searches the request's header list for a header with the specified name,
* using a case-insensitive comparison.
*
* @param req Pointer to the parsed HTTP request.
* @param name Name of the header to retrieve.
*
* @return Pointer to the header value if found, or NULL if not present.
*
* @note Header name comparison is case-insensitive.
* @note The returned pointer refers to internal storage within the request
*       structure and must not be modified or freed.
*/
const char *http_get_header(const http_request *req, const char *name) {
  for (size_t i = 0; i < req->header_count; ++i) {
    if (ci_equal(req->headers[i].name, name)) {
      return req->headers[i].value;
    }
  }
  return NULL;
}

/**
* @brief Determine the MIME type based on a file path extension.
*
* Extracts the file extension from the given path and returns a
* corresponding MIME type string. If the extension is not recognized,
* a default binary MIME type is returned.
*
* @param path Path to the file.
*
* @return A string representing the MIME type (e.g., "text/html").
*
* @note The returned string is a constant and must not be modified.
* @note If no file extension is present, "application/octet-stream"
*       is returned.
*/
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
