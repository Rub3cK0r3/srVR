# 🖥 srVR

![Release](https://img.shields.io/github/v/release/Rub3cK0r3/srVR)
![C](https://img.shields.io/badge/language-C-blue)
![License](https://img.shields.io/badge/license-educational-lightgrey)
![OS](https://img.shields.io/badge/OS-POSIX-orange)

**srVR** is a minimal **HTTP server** written in plain C, designed to teach how HTTP and TCP sockets work under the hood, without frameworks or external libraries.

It is tiny, simple, and educational — perfect for learning low-level networking and HTTP basics. The project demonstrates two distinct concurrency models: **thread-per-connection** and **event-driven (epoll)**.

---

## 📍 Overview

srVR demonstrates fundamental concepts in network programming:

* **TCP socket programming** in C (POSIX standards)
* **HTTP request parsing** and response handling
* **Static file serving** (HTML, CSS, images, etc.)
* **Concurrent connection handling** using two different models:
  - Thread-per-connection model (traditional approach)
  - Event-driven model using `epoll()` (modern Linux approach)
* **Graceful shutdown** with signal handling (SIGINT)
* **Configuration management** from config files
* **Structured logging** with timestamps

It is **not** a production-ready server — this is intentional. The focus is on teaching fundamentals and best practices, not implementing the full HTTP/1.1 specification.

---

## ⚙ Features

* **Zero external dependencies** — uses only POSIX APIs
* **Static file serving** — HTTP GET/HEAD/POST for files
* **Simple HTTP parsing** — enough to understand the protocol
* **Dual concurrency models**:
  - Thread-based (`server_run_threaded`) — one OS thread per connection
  - Event-based (`server_run_epoll`) — single thread with Linux epoll
* **Configuration file support** — simple key=value format
* **Structured logging** — timestamped log file (`server.log`)
* **Security basics** — directory traversal protection
* **Clean code** — extensively commented, POSIX-compliant, easy to extend

---

## 🛠 Requirements

* **POSIX-compliant system**: Linux, macOS, BSD
* **C compiler**: GCC, Clang, or any standard C99-compatible compiler
* **Linux epoll support** (for epoll mode): Linux 2.6+ with libc support

---

## 🚀 Build & Run

### Quick Start

```bash
# Build the server
make

# Run with default settings (port 8080, ./www directory)
./bin/srVR
```

### Manual Build

```bash
# Compile all sources
gcc -pthread -o srVR \
  src/main.c \
  src/server.c \
  src/router.c \
  src/http.c \
  src/ev.c
```

### Configuration

Create or modify `config/server.conf`:

```conf
# Server listening port (default: 8080)
port=8080

# Document root directory (default: ./www)
document_root=./www

# Enable event-driven epoll mode (0=disabled, 1=enabled)
use_epoll=1
```

### Run the Server

#### Basic Usage
```bash
# Start with default configuration (thread-based, port 8080)
./bin/srVR
```

#### With Thread-Based Model
```bash
# Edit config to use thread-based concurrency
echo "use_epoll=0" > config/server.conf
echo "port=8080" >> config/server.conf
echo "document_root=./www" >> config/server.conf

# Run the server
./bin/srVR

# The server will accept connections and spawn one thread per client
# Access via browser: http://localhost:8080
```

#### With Event-Driven Model (Epoll)
```bash
# Edit config to use epoll
echo "use_epoll=1" > config/server.conf
echo "port=8080" >> config/server.conf
echo "document_root=./www" >> config/server.conf

# Run the server
./bin/srVR

# Single thread handles all clients via epoll
# Scales to 10,000+ concurrent connections
# Access via browser: http://localhost:8080
```

#### Custom Port and Root Directory
```bash
# Create custom configuration
cat > config/server.conf << EOF
port=9000
document_root=./public
use_epoll=1
EOF

# Run on port 9000
./bin/srVR
```

#### Testing with curl
```bash
# GET request
curl -v http://localhost:8080/index.html

# HEAD request (no body)
curl -I http://localhost:8080/

# POST request with data
curl -X POST -d "username=test&password=secret" http://localhost:8080/

# Follow redirects
curl -L http://localhost:8080/

# Add custom headers
curl -H "User-Agent: CustomClient" http://localhost:8080/
```

#### Background Execution
```bash
# Run server in background
./bin/srVR &

# Get the process ID
SERVER_PID=$!

# Do other tasks...

# Stop server gracefully
kill -SIGINT $SERVER_PID

# Check logs
tail -f server.log
```

### Stop the Server

Press `Ctrl+C` (SIGINT) for graceful shutdown. The server will:
1. Stop accepting new connections
2. Allow existing connections to complete
3. Close all sockets
4. Exit cleanly

```bash
# In terminal where server is running
Ctrl+C

# Or from another terminal
kill -SIGINT $(pgrep -f "bin/srVR")
```

---

## 📚 Architecture

### Thread-Based Model (`use_epoll=0`)

The default mode uses a **thread-per-connection** architecture:

1. Main thread accepts incoming connections
2. For each connection, spawn a new detached thread
3. Thread handles the complete HTTP request/response cycle
4. Thread cleans up and exits
5. Main thread continues accepting

**Pros:**
- Simple and straightforward to understand
- Each connection is independent
- Clean separation of concerns

**Cons:**
- Context switching overhead with many connections
- Not suitable for very high concurrency (thousands of connections)

### Event-Driven Model (`use_epoll=1`)

The epoll-based mode uses a **single-threaded event loop**:

1. Main thread creates an epoll instance
2. Registers listening socket for `EPOLLIN` events
3. Waits for events (new connections, readable sockets, errors)
4. For new connections, registers client socket with epoll
5. When client socket becomes readable, parses HTTP request
6. Processes request and sends response
7. Handles disconnections and errors

**Pros:**
- Scales to thousands of concurrent connections
- Lower memory overhead (no per-thread stacks)
- Efficient CPU utilization
- Modern, production-inspired design

**Cons:**
- More complex state management
- Requires non-blocking I/O understanding
- Harder to debug

---

## 📂 Project Structure

```
srVR/
├── Makefile                 # Build rules
├── README.md               # This file
├── LICENSE                 # Project license
├── compile_commands.json   # Clangd configuration
├── config/
│   └── server.conf        # Configuration file
├── include/
│   ├── server.h           # Core server interfaces
│   ├── router.h           # Request routing
│   ├── http.h             # HTTP parsing
│   └── ev.h               # Event loop structures
├── src/
│   ├── main.c             # Entry point
│   ├── server.c           # Server core + threading/epoll
│   ├── router.c           # HTTP request handling
│   ├── http.c             # HTTP parsing utilities
│   └── ev.c               # Event loop wrapper (optional)
├── www/
│   └── index.html         # Default served file
└── bin/                   # Compiled binaries

```

---

## 📚 Educational Value

### Perfect for Learning

* **TCP/IP fundamentals** — socket creation, binding, listening, accepting
* **HTTP protocol** — request parsing, response formatting, status codes
* **Concurrency patterns** — understanding threads vs. event loops
* **Linux system calls** — `epoll()`, `fcntl()`, `sendfile()`, signal handling
* **C best practices** — memory management, error handling, secure coding
* **Network security** — directory traversal protection, input validation

### Key Concepts Taught

| Concept | Location | Details |
|---------|----------|---------|
| Socket creation | `server_listen()` | IPv4 TCP socket setup |
| Accept loop | `server_run_threaded()` | Blocking accept pattern |
| Threading | `client_thread_main()` | Thread-per-connection model |
| Event loop | `server_run_epoll()` | Non-blocking I/O with epoll |
| HTTP parsing | `http_parse_request()` | Request line and headers |
| Static file serving | `serve_static_file()` | Zero-copy with sendfile() |
| Signal handling | `main.c` | Graceful shutdown |
| Logging | `log_info/warn/error()` | Structured logging |

---

## 🔧 Extending srVR

### Add a New Route

Edit `src/router.c` and modify `handle_client_connection()`:

```c
if (strcmp(req.method, "GET") == 0) {
    if (strcmp(req.path, "/api/status") == 0) {
        send_simple_response(clientfd, 200, "OK", 
                           "{\"status\":\"running\"}", 
                           "application/json");
    } else {
        serve_static_file(clientfd, cfg, req.path);
    }
}
```

### Change the Listening Port

Edit `config/server.conf`:

```conf
port=9000
```

### Modify Response Headers

Edit `send_simple_response()` in `src/router.c` to add custom headers.

---

## 🧪 Testing

The project includes a test suite to verify both concurrency models:

```bash
# Run all tests
make test

# Run epoll tests specifically
make test-epoll
```

See `tests/` directory for test implementations.

---

## 🐛 Debugging

### Enable Verbose Logging

The server logs all requests and errors to `server.log`:

```bash
tail -f server.log
```

### Test with curl

```bash
# GET request
curl -v http://localhost:8080/index.html

# POST request
curl -X POST -d "test data" http://localhost:8080/

# HEAD request
curl -I http://localhost:8080/
```

### Use strace for System Calls

```bash
strace -e trace=network,accept,read,write ./bin/srVR
```

---

## 📜 License

This project is licensed for educational purposes. See [LICENSE](LICENSE) for details.

---

## 🤝 Contributing

Contributions are welcome! Please:

1. Keep code clean and well-commented
2. Maintain POSIX compliance
3. Add tests for new features
4. Update documentation

---

## 💡 Future Enhancements

* HTTP/1.1 Keep-Alive support
* HTTPS/TLS encryption
* Multi-threaded epoll (using thread pools)
* Performance benchmarking suite
* More comprehensive HTTP parsing
* JSON configuration format
