/*
 * Epoll wrapper interfaces and configuration.
 * ev.h
 */

#ifndef SRVR_EPOLL_WRAPPER_H
#define SRVR_EPOLL_WRAPPER_H

#include "http.h"

/* Client states in the epoll event loop */
#define EPOLL_CLIENT_READING    0
#define EPOLL_CLIENT_WRITING    1
#define EPOLL_CLIENT_CLOSING    2

/*
 * State container for an epoll-managed client connection.
 * 
 * Tracks:
 *  - Socket file descriptor
 *  - Receive buffer and its current length
 *  - Parsed HTTP request (when available)
 *  - Current state (reading, writing, or closing)
 *  - Response data and write position (for streaming)
 */
typedef struct epoll_client {
    int clientfd;                    /* client socket file descriptor */
    char buff[4096];                 /* 4KB buffer for HTTP request */
    int buffer_len;                  /* bytes currently in buff */
    int state;                       /* EPOLL_CLIENT_* state */
    http_request req;                /* parsed HTTP request */
    int request_parsed;              /* 1 if req has been parsed */
    char *response_body;             /* dynamically allocated response data */
    size_t response_size;            /* size of response_body */
    size_t response_written;         /* bytes already sent */
} epoll_client;

/*
 * Check if an HTTP request is complete (headers terminated by \r\n\r\n).
 *
 * @param buffer Pointer to the HTTP request buffer.
 * @param len    Length of data in the buffer.
 *
 * @return 1 if the request headers are complete, 0 otherwise.
 */
int is_http_request_complete(const char *buffer, size_t len);

#endif
