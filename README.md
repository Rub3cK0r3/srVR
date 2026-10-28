# srVR

srVR is a small HTTP server written in C11. It is an educational project for exploring TCP sockets, HTTP parsing, file serving, and two concurrency models without a web framework.

It is a working MVP, not a production server. The implementation focuses on being inspectable while practicing safer systems programming: bounded request and file sizes, explicit connection states, non-blocking I/O in epoll mode, error handling, and executable integration tests. These measures are learning examples, not a security or production guarantee.

## Stack

- C11 and the standard C library
- POSIX sockets, files, signals, and pthreads
- Linux `epoll` for the event-driven mode
- Make for builds and tests; no third-party runtime libraries

The server can run one thread per connection (`use_epoll=0`) or use a non-blocking, event-driven loop (`use_epoll=1`). The epoll mode is Linux-specific; the threaded mode uses POSIX APIs.

## Build and Run

Requirements: GCC or Clang, Make, and Linux for epoll mode.

```sh
make
./srVR
```

The server reads `config/server.conf` from the current working directory. The default configuration listens on port `8080`, serves files from `./www`, and enables epoll:

```conf
port=8080
root=./www
use_epoll=1
```

Set `use_epoll=0` to use the thread-per-connection mode. Press `Ctrl+C` to stop the server. Requests can be sent from another terminal:

```sh
curl -i http://127.0.0.1:8080/
```

## Tests

```sh
make test
```

This runs the C parser and live-server tests, then builds and launches the server for additional HTTP smoke tests. The smoke test uses `curl` and `python3`, an ephemeral port, and a temporary working directory; it does not overwrite the project configuration. Run either layer separately with `make test-epoll` or `make test-server`.

## Scope

The MVP serves static files and demonstrates basic HTTP request and response handling. It does not implement the full HTTP specification, TLS, chunked transfer encoding, or production-grade connection management. The code and tests are intended to make those boundaries visible and provide a base for learning, not to replace a hardened web server.

See [LICENSE](LICENSE) for licensing details.