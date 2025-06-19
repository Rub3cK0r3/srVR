# srVR

Simple HTTP server in C. Nothing fancy, just raw socket handling and file serving.

---

## What is it?

srVR is a tiny HTTP server written in plain C using POSIX sockets.  
It listens on a port, accepts connections, parses minimal HTTP GET requests, and serves static files from a folder.

If you want to understand how HTTP and sockets work under the hood without frameworks, this is for you.

---

## Features

- Raw TCP sockets, no external libs  
- Serves static files (html, css, images)  
- Handles only GET requests, minimal HTTP parsing  
- No threading or advanced features, just basics  

---

## Requirements

- POSIX compliant system (Linux, macOS, etc.)  
- GCC or any C compiler  

---

## Build & Run

```bash
gcc -o srVR src/srVR.c
./srVR 8080 ./public
