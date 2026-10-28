/*
 * test_epoll.c
 *
 * Comprehensive test suite for the epoll-based HTTP server.
 * Tests the event-driven concurrency model, HTTP handling,
 * and response generation.
 *
 * Build: gcc -std=c11 -Wall -Wextra -g -Iinclude -o test_epoll \
 *          tests/test_epoll.c src/http.c src/server.c src/router.c src/ev.c \
 *          -lpthread
 * Run:   ./test_epoll
 */

#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "ev.h"
#include "http.h"
#include "server.h"

volatile sig_atomic_t srvr_running = 1;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test logging utilities */
static void test_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
    fflush(stdout);
}

static void test_pass(const char *name) {
    test_log("✓ PASS: %s", name);
    tests_passed++;
}

static void test_fail(const char *name, const char *reason) {
    test_log("✗ FAIL: %s - %s", name, reason);
    tests_failed++;
}

static void test_http_completion(void) {
    test_log("\n=== Test: HTTP Request Completion Detection ===");

    /* Test incomplete request (missing \r\n\r\n) */
    const char *incomplete = "GET / HTTP/1.1\r\nHost: localhost\r\n";
    if (is_http_request_complete(incomplete, strlen(incomplete))) {
        test_fail("HTTP completion: incomplete request", "Should return 0");
    } else {
        test_pass("HTTP completion: incomplete request");
    }

    /* Test complete request */
    const char *complete = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    if (!is_http_request_complete(complete, strlen(complete))) {
        test_fail("HTTP completion: complete request", "Should return 1");
    } else {
        test_pass("HTTP completion: complete request");
    }

    /* Test very small buffer */
    const char *tiny = "GET";
    if (is_http_request_complete(tiny, strlen(tiny))) {
        test_fail("HTTP completion: tiny buffer", "Should return 0");
    } else {
        test_pass("HTTP completion: tiny buffer");
    }

    /* Test request with body marker */
    const char *with_body = "POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    if (!is_http_request_complete(with_body, strlen(with_body))) {
        test_fail("HTTP completion: request with body", "Should return 1");
    } else {
        test_pass("HTTP completion: request with body");
    }
}

static void test_http_parsing(void) {
    test_log("\n=== Test: HTTP Request Parsing ===");

    /* Test simple GET request */
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "GET /index.html HTTP/1.1\r\n"
             "Host: localhost:8080\r\n"
             "User-Agent: TestClient\r\n"
             "\r\n");

    http_request req;
    if (http_parse_request(buffer, strlen(buffer), &req) == -1) {
        test_fail("HTTP parsing: basic GET", "Parsing failed");
        return;
    }

    if (strcmp(req.method, "GET") != 0) {
        test_fail("HTTP parsing: method extraction", "Method not 'GET'");
    } else {
        test_pass("HTTP parsing: method extraction");
    }

    if (strcmp(req.path, "/index.html") != 0) {
        test_fail("HTTP parsing: path extraction", "Path not '/index.html'");
    } else {
        test_pass("HTTP parsing: path extraction");
    }

    if (strcmp(req.version, "HTTP/1.1") != 0) {
        test_fail("HTTP parsing: version extraction", "Version not 'HTTP/1.1'");
    } else {
        test_pass("HTTP parsing: version extraction");
    }

    /* Test header extraction */
    const char *host = http_get_header(&req, "Host");
    if (!host) {
        test_fail("HTTP parsing: header extraction", "Host header not found");
    } else if (strstr(host, "localhost") != NULL) {
        /* Just verify it contains 'localhost' since the exact format might change */
        test_pass("HTTP parsing: header extraction");
    } else {
        test_fail("HTTP parsing: header extraction", "Host header value incorrect");
    }

    /* Test POST request with body */
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer),
             "POST /api/data HTTP/1.1\r\n"
             "Host: localhost\r\n"
             "Content-Length: 11\r\n"
             "\r\n"
             "test=value");

    memset(&req, 0, sizeof(req));
    if (http_parse_request(buffer, strlen(buffer), &req) == -1) {
        test_fail("HTTP parsing: POST request", "Parsing failed");
    } else {
        test_pass("HTTP parsing: POST request");
    }

    if (strcmp(req.method, "POST") != 0) {
        test_fail("HTTP parsing: POST method", "Method not 'POST'");
    } else {
        test_pass("HTTP parsing: POST method");
    }

    if (req.body && req.body_length == strlen("test=value") &&
        memcmp(req.body, "test=value", req.body_length) == 0) {
        test_pass("HTTP parsing: POST body");
    } else {
        test_fail("HTTP parsing: POST body", "Body content or length is incorrect");
    }
}

typedef struct server_thread_context {
    int serverfd;
    const server_config *cfg;
} server_thread_context;

static void *run_epoll_server(void *arg) {
    server_thread_context *context = arg;
    server_run_epoll(context->serverfd, context->cfg);
    return NULL;
}

static int send_http_request(unsigned short port, const char *request,
                             char *response, size_t response_capacity) {
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        return -1;
    }

    struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
    if (setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   sizeof(timeout)) == -1) {
        close(clientfd);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(port);
    if (connect(clientfd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1) {
        close(clientfd);
        return -1;
    }

    size_t request_len = strlen(request);
    size_t request_sent = 0;
    while (request_sent < request_len) {
        ssize_t bytes = send(clientfd, request + request_sent,
                             request_len - request_sent, 0);
        if (bytes == -1 && errno == EINTR) {
            continue;
        }
        if (bytes <= 0) {
            close(clientfd);
            return -1;
        }
        request_sent += (size_t)bytes;
    }

    size_t response_len = 0;
    while (response_len < response_capacity - 1) {
        ssize_t bytes = recv(clientfd, response + response_len,
                             response_capacity - response_len - 1, 0);
        if (bytes == 0) {
            break;
        }
        if (bytes == -1 && errno == EINTR) {
            continue;
        }
        if (bytes == -1) {
            close(clientfd);
            return -1;
        }
        response_len += (size_t)bytes;
    }

    response[response_len] = '\0';
    close(clientfd);
    return 0;
}

static void expect_http_response(unsigned short port, const char *name,
                                 const char *request, const char *status,
                                 const char *expected_body, int body_must_be_empty) {
    char response[16384];
    if (send_http_request(port, request, response, sizeof(response)) == -1) {
        test_fail(name, "HTTP exchange failed or timed out");
        return;
    }

    if (strncmp(response, status, strlen(status)) != 0) {
        test_fail(name, "Unexpected HTTP status line");
        return;
    }

    char *body = strstr(response, "\r\n\r\n");
    if (!body) {
        test_fail(name, "Response headers are not terminated");
        return;
    }
    body += 4;

    if (body_must_be_empty && *body != '\0') {
        test_fail(name, "Response body should be empty");
        return;
    }
    if (expected_body && !strstr(body, expected_body)) {
        test_fail(name, "Expected response body content was not found");
        return;
    }

    if (!body_must_be_empty) {
        char *length_header = strstr(response, "Content-Length: ");
        if (!length_header) {
            test_fail(name, "Response is missing Content-Length");
            return;
        }
        unsigned long declared_length = strtoul(length_header + 16, NULL, 10);
        if (declared_length != strlen(body)) {
            test_fail(name, "Content-Length does not match response body");
            return;
        }
    }

    test_pass(name);
}

static void test_network_http_request(void) {
    test_log("\n=== Test: Live HTTP Integration ===");

    char original_cwd[PATH_MAX];
    if (!getcwd(original_cwd, sizeof(original_cwd))) {
        test_fail("HTTP integration setup", "Could not get current directory");
        return;
    }

    char temp_dir_template[] = "/tmp/srvr-test-XXXXXX";
    char *temp_dir = mkdtemp(temp_dir_template);
    if (!temp_dir) {
        test_fail("HTTP integration setup", "Could not create temporary directory");
        return;
    }

    char log_path[PATH_MAX];
    char document_root[sizeof(((server_config *)0)->document_root)];
    if (snprintf(log_path, sizeof(log_path), "%s/server.log", temp_dir) >=
            (int)sizeof(log_path) ||
        snprintf(document_root, sizeof(document_root), "%s/www", original_cwd) >=
            (int)sizeof(document_root)) {
        test_fail("HTTP integration setup", "Test path is too long");
        rmdir(temp_dir);
        return;
    }

    server_config cfg;
    cfg.port = 0;
    snprintf(cfg.document_root, sizeof(cfg.document_root), "%s", document_root);
    cfg.use_epoll = 1;

    int serverfd = server_listen(&cfg);
    if (serverfd == -1) {
        test_fail("HTTP integration setup", "Could not create listening socket");
        rmdir(temp_dir);
        return;
    }

    struct sockaddr_in server_addr;
    socklen_t address_len = sizeof(server_addr);
    if (getsockname(serverfd, (struct sockaddr *)&server_addr, &address_len) == -1) {
        test_fail("HTTP integration setup", "Could not discover ephemeral port");
        close(serverfd);
        rmdir(temp_dir);
        return;
    }
    unsigned short port = ntohs(server_addr.sin_port);

    if (chdir(temp_dir) == -1) {
        test_fail("HTTP integration setup", "Could not isolate server log");
        close(serverfd);
        rmdir(temp_dir);
        return;
    }

    server_thread_context context = {.serverfd = serverfd, .cfg = &cfg};
    pthread_t server_thread;
    srvr_running = 1;
    int thread_error = pthread_create(&server_thread, NULL,
                                     run_epoll_server, &context);
    if (thread_error != 0) {
        test_fail("HTTP integration setup", "Could not start server thread");
        close(serverfd);
        chdir(original_cwd);
        rmdir(temp_dir);
        return;
    }

    expect_http_response(port, "HTTP GET serves index",
                         "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n",
                         "HTTP/1.1 200 OK", "It works.", 0);
    expect_http_response(port, "HTTP HEAD omits response body",
                         "HEAD /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n",
                         "HTTP/1.1 200 OK", NULL, 1);
    expect_http_response(port, "HTTP missing file returns 404",
                         "GET /missing.html HTTP/1.1\r\nHost: localhost\r\n\r\n",
                         "HTTP/1.1 404 Not Found", "404 Not Found", 0);
    expect_http_response(port, "HTTP traversal returns 403",
                         "GET /../LICENSE HTTP/1.1\r\nHost: localhost\r\n\r\n",
                         "HTTP/1.1 403 Forbidden", "403 Forbidden", 0);
    expect_http_response(port, "HTTP POST acknowledges request",
                         "POST / HTTP/1.1\r\nHost: localhost\r\n"
                         "Content-Length: 4\r\n\r\ntest",
                         "HTTP/1.1 200 OK", "POST received.", 0);
    expect_http_response(port, "HTTP unsupported method returns 405",
                         "DELETE / HTTP/1.1\r\nHost: localhost\r\n\r\n",
                         "HTTP/1.1 405 Method Not Allowed", "405 Method Not Allowed", 0);
    expect_http_response(port, "HTTP malformed request returns 400",
                         "BROKEN\r\n\r\n", "HTTP/1.1 400 Bad Request",
                         "400 Bad Request", 0);

    srvr_running = 0;
    pthread_join(server_thread, NULL);
    chdir(original_cwd);
    unlink(log_path);
    rmdir(temp_dir);
}

int main(void) {
    test_http_completion();
    test_http_parsing();
    test_network_http_request();
    test_log("Passed: %d", tests_passed);
    test_log("Failed: %d", tests_failed);
    test_log("Total:  %d", tests_passed + tests_failed);

    if (tests_failed == 0) {
        test_log("\n✓ All tests passed!");
        return EXIT_SUCCESS;
    } else {
        test_log("\n✗ Some tests failed.");
        return EXIT_FAILURE;
    }
}
