# Concurrency Models Comparison

This document explains the differences between the two concurrency models in srVR.

## Thread-Per-Connection Model (use_epoll=0)

### How It Works
```
Client 1 ──┐
Client 2 ──┼──> Main Thread ──> Spawn Thread 1 ──> Handle Client 1
Client 3 ──┤                 ──> Spawn Thread 2 ──> Handle Client 2
Client 4 ──┘                 ──> Spawn Thread 3 ──> Handle Client 3
            ...                  ──> Spawn Thread 4 ──> Handle Client 4
```

### Flow
1. Main thread calls `accept()` blocking on listening socket
2. For each incoming connection, create a new detached thread
3. Thread handles complete request/response cycle
4. Thread cleans up and exits

### Code Location
- `src/server.c`: `server_run_threaded()`
- `src/server.c`: `client_thread_main()`

### Characteristics
| Aspect | Details |
|--------|---------|
| **Memory per connection** | ~1-2 MB (thread stack + TLS) |
| **Max connections** | ~100-500 (OS dependent) |
| **Context switching** | High overhead |
| **CPU utilization** | Lower for high concurrency |
| **Simplicity** | Very simple, easy to debug |
| **Blocking calls** | Allowed and common |

### Suitable For
- Learning purposes
- Low-traffic applications
- Small deployments
- Simple debugging

---

## Event-Driven Model (use_epoll=1)

### How It Works
```
         Non-blocking I/O
Client 1 ──┐
Client 2 ──┼──> Main Thread ──> epoll_wait() ──┐
Client 3 ──┤                                    ├──> Handle Ready Clients
Client 4 ──┘                                    └──> Process Events
...          (Single thread, multiple clients)
```

### Flow
1. Create epoll instance
2. Register listening socket for EPOLLIN
3. Enter event loop: `epoll_wait()`
4. When events occur:
   - New connection: Accept and register client socket
   - Client readable: Parse HTTP request
   - Client writable: Send HTTP response
5. Close client when done

### Code Location
- `src/server.c`: `server_run_epoll()`
- `src/server.c`: `epoll_handle_request()`
- `src/server.c`: `epoll_send_response()`

### Characteristics
| Aspect | Details |
|--------|---------|
| **Memory per connection** | ~40-100 bytes |
| **Max connections** | ~10,000-100,000 |
| **Context switching** | Minimal |
| **CPU utilization** | Very efficient |
| **Complexity** | More complex state machine |
| **Blocking calls** | NOT allowed, must be non-blocking |

### Suitable For
- Production servers
- High-traffic applications
- Large deployments
- Modern performance-sensitive systems

---

## State Machine in Epoll Mode

```
┌─────────────┐
│   READING   │  Client socket is readable
└──────┬──────┘
       │ HTTP request complete?
       ├─ No:  Continue reading
       └─ Yes: Parse & Route
              ↓
┌─────────────┐
│   WRITING   │  Send HTTP response
└──────┬──────┘
       │ All data sent?
       ├─ No:  Continue writing
       └─ Yes: Close connection
              ↓
┌─────────────┐
│   CLOSING   │  Clean up & free resources
└─────────────┘
```

---

## Key Differences

### Synchronous (Thread-Based)
```c
// Blocking model
while (running) {
    int clientfd = accept(serverfd);  // Blocks until connection
    create_thread(handle_client, clientfd);
}

void handle_client(int fd) {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);  // Blocks until data
    // Process request
    send(fd, response, len, 0);  // Blocks until sent
    close(fd);
}
```

### Asynchronous (Epoll-Based)
```c
// Event-driven model
int epfd = epoll_create1(0);
epoll_ctl(epfd, EPOLL_CTL_ADD, serverfd, &ev);

while (running) {
    int n = epoll_wait(epfd, events, MAX_EVENTS, timeout);  // Wait for events
    for (int i = 0; i < n; i++) {
        if (events[i].data.fd == serverfd) {
            // New connection
            int clientfd = accept(serverfd, ...);  // Non-blocking
            // Register client
        } else {
            // Existing client
            ssize_t n = read(events[i].data.fd, buf, len);  // Non-blocking
            // Process if complete
        }
    }
}
```

---

## Performance Implications

### Thread-Based (100 concurrent connections)
- Memory: ~100 MB (100 threads × 1 MB)
- Context switches: High (~100-1000 per second)
- Response latency: Variable

### Epoll-Based (100 concurrent connections)
- Memory: ~10 KB (100 clients × 100 bytes)
- Context switches: Low (~1-10 per second)
- Response latency: Consistent

---

## When to Use Each

### Use Thread-Per-Connection When:
- Building a learning project
- Number of concurrent connections is small (<100)
- You need simple, easy-to-debug code
- Thread safety is not a major concern

### Use Epoll When:
- Building a production server
- Expecting many concurrent connections (>1000)
- You need maximum throughput and efficiency
- You understand event-driven programming

---

## Configuration

To switch between models, edit `config/server.conf`:

```conf
# Thread-based (default)
use_epoll=0

# Event-driven
use_epoll=1
```

Both models serve files correctly and handle all HTTP methods (GET, HEAD, POST).
The main difference is how they handle concurrent connections.

---

## Testing Performance

### Thread-Based
```bash
# This will use thread-per-connection
echo "use_epoll=0" > config/server.conf
./bin/srVR &

# Test with many clients
ab -n 1000 -c 100 http://localhost:8080/index.html
```

### Epoll-Based
```bash
# This will use epoll event loop
echo "use_epoll=1" > config/server.conf
./bin/srVR &

# Test with many clients
ab -n 1000 -c 100 http://localhost:8080/index.html
```

The epoll version should handle more concurrent connections with lower latency.
