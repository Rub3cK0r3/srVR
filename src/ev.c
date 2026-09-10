#include <stdio.h>
#include <string.h>

#include "ev.h"

/**
 * @brief Check if an HTTP request is complete.
 *
 * An HTTP request is considered complete when the header section
 * ends with the sequence "\r\n\r\n" (carriage return, line feed,
 * repeated). This marks the boundary between HTTP headers and
 * the optional request body.
 *
 * @param buffer Pointer to the HTTP request buffer.
 * @param len    Length of data currently in the buffer.
 *
 * @return 1 if the request headers are complete, 0 otherwise.
 *
 * @note This function only checks for header completion. For POST
 *       requests with a body, the caller should use Content-Length
 *       to read additional data.
 */
int is_http_request_complete(const char *buffer, size_t len) {
    if (len < 4) {
        return 0;
    }
    return strstr(buffer, "\r\n\r\n") != NULL;
}
