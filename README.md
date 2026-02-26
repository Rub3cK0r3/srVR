# 🖥 srVR

![C](https://img.shields.io/badge/language-C-blue)
![License](https://img.shields.io/badge/license-educational-lightgrey)
![OS](https://img.shields.io/badge/OS-POSIX-orange)

**srVR** is a minimal **HTTP server** written in plain C, designed to teach how HTTP and TCP sockets work under the hood, without frameworks or external libraries.

It is tiny, simple, and educational — perfect for learning low-level networking and HTTP basics.

---

## 📌 Overview

srVR demonstrates:

* TCP socket programming in C (POSIX)
* Minimal HTTP parsing
* Static file serving (HTML, CSS, images)
* Single-threaded request handling

It is **not** a production-ready server — no concurrency, no advanced HTTP features, just the fundamentals.

---

## ⚙ Features

* Raw TCP sockets, no external dependencies
* Serves static files from a folder
* Handles only HTTP GET requests
* Minimal HTTP parsing: only enough to serve files
* Educational focus — easy to read and extend

---

## 🛠 Requirements

* POSIX-compliant system: Linux, macOS, BSD
* GCC or any standard C compiler

---

## 🚀 Build & Run

```bash
gcc -o srVR src/srVR.c
./srVR
```

* The server listens on a specified port (default in code)
* Place your static files (HTML, CSS, images) in the serving folder
* Access from your browser: `http://localhost:<port>/index.html`

---

## 🧠 How It Works

1. Create a TCP socket using `socket()`
2. Bind to a port with `bind()`
3. Listen for connections with `listen()`
4. Accept connections with `accept()`
5. Parse minimal HTTP GET request
6. Read requested file from disk
7. Send file contents back over the socket
8. Close connection

This exposes the raw mechanics of how HTTP servers function at a low level.

---

## ⚠️ Limitations

* Only supports **GET** requests
* No concurrency (single-threaded)
* No HTTP headers beyond minimal requirement
* No MIME type detection beyond basic handling
* Educational use only — not secure for production

---

## 📚 Educational Value

srVR is ideal for:

* Understanding raw TCP socket communication
* Learning HTTP request/response mechanics
* Practicing C network programming
* Gaining insight into server architecture without frameworks

---

## 🔮 Possible Improvements

* Add **multi-threading** for concurrent connections
* Support more HTTP methods (POST, PUT, etc.)
* Implement MIME type detection
* Add logging of requests
* Improve HTTP header parsing
* Add HTTPS support via OpenSSL

## Build & Run with...

```bash
gcc -o srVR src/srVR.c
./srVR
