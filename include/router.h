/*
 * Simple request router for srVR.
 *
 * The router decides how to handle a parsed HTTP request:
 * - serve a static file from the configured document root
 * - handle POST bodies
 * - return error responses (404, 403, 500, ...)
 */

#ifndef SRVR_ROUTER_H
#define SRVR_ROUTER_H

#include "http.h"
#include "server.h"

/*
 * Handle a single client connection:
 * - read and parse the HTTP request
 * - route based on method + path
 * - send back an HTTP response
 *
 * The function is responsible for handling partial TCP reads
 * and writes and for printing any POST request body to the
 * console for educational purposes.
 */
void handle_client_connection(int clientfd, const server_config *cfg);

#endif /* SRVR_ROUTER_H */

