/*
 * Epoll wrapper interfaces and configuration.
 * ev.h
 */

#ifndef SRVR_EPOLL_WRAPPER_H
#define SRVR_EPOLL_WRAPPER_H
typedef struct epoll_client {
    int clientfd; // client socket file descriptor
    char buff[4096]; // 4KB client buffer for read/write operations
    int buffer_len;
    int state; // example : READING is 0 and WRITING is 1
} epoll_client ;
#endif
