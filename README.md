# 🖥 srVR

![Release](https://img.shields.io/github/v/release/Rub3cK0r3/srVR)
![C](https://img.shields.io/badge/language-C-blue)
![License](https://img.shields.io/badge/license-educational-lightgrey)
![OS](https://img.shields.io/badge/OS-POSIX-orange)

**srVR** is a minimal **HTTP server** written in plain C, designed to teach how HTTP and TCP sockets work under the hood, without frameworks or external libraries.

It is tiny, simple, and educational — perfect for learning low-level networking and HTTP basics.

---

## 📍 Overview

srVR demonstrates:

* TCP socket programming in C (POSIX)
* Minimal HTTP parsing
* Static file serving (HTML, CSS, images)
* Single-threaded request handling and optional event based implementation

It is **not** a production-ready server, no advanced HTTP features yet, just the fundamentals.

---

## ⚙ Features

* Raw TCP sockets, no external dependencies
* Serves static files from a folder
* Handles HTTP GET - POST requests
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

## 📚 Educational Value

srVR is ideal for:

* Understanding raw TCP socket communication
* Learning HTTP request/response mechanics
* Practicing C network programming
* Gaining insight into server architecture without frameworks
