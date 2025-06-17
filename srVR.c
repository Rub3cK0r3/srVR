/*
 * Rub3cK0r3 - my own http server
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "srVR.h"

// Function to initialize connection from serverfd giving the sockaddr_in and its size
int init_connection(int serverfd, struct sockaddr_in *myaddr,
                    socklen_t addr_size) {
  /*
   * When  a  socket  is  created  with socket(2), it exists in a name space
   * (address family) but has no address assigned to it.  bind()  assigns the
   * address specified by addr to the socket referred to by the file descriptor
   * sockfd.  addrlen specifies the size, in bytes, of the address structure
   * pointed to by addr.  Traditionally, this operation  is  called  “assigning
   * a name to a socket”. Normalmente, es necesario asignar una dirección local
   * usando bind() a un conector SOCK_STREAM antes de que éste pueda recibir
   * conexiones (vea accept(2)).
   */

  if (bind(serverfd, (struct sockaddr *)myaddr, addr_size) == -1) {
    perror("Error : Socket has not been bound");
    return -1;
  }
  /*
   * listen()  marks  the socket referred to by serverfd as a passive
   * socket, that is, as a socket that will be used to accept incoming
   * connection requests using accept(2).
   *
   * The serverfd argument is a file descriptor that refers to a socket of type
   * SOCK_STREAM or SOCK_SEQPACKET.
   *
   * The backlog argument defines the maximum length to which the queue of
   * pending connections for serverfd may grow.
   */

  if (listen(serverfd, 1) == -1) {
    perror("Error : Could not listen");
    return -1;
  }
  return serverfd;
}

// It returns the serverfd, already working for accepting the cliendtf
int init_server() {
  /*
   * File descriptor is an integer in your application that refers to the file
   * description in the kernel.
   *
   * File description is the structure in the kernel that maintains the state of
   * an open file (its current position, blocking/non-blocking, etc.). In Linux
   * file description is struct file. *POSIX open():
   *
   * The open() function shall establish the connection between a file and a
   * file descriptor. It shall create an open file description that refers to a
   * file and a file descriptor that refers to that open file description. The
   * file descriptor is used by other I/O functions to refer to that file. The
   * path argument points to a pathname naming the file. The open() function
   * shall return a file descriptor for the named file that is the lowest file
   * descriptor not currently open for that process. The open file description
   * is new, and therefore the file descriptor shall not share it with any other
   * process in the system.
   */
  int serverfd, clientfd;
  struct sockaddr_in myaddr, clientaddr;
  /*
   *  struct sockaddr_in {
   *       sa_family_t     sin_family;     //AF_INET
   *       in_port_t       sin_port;       //Port number
   *       struct in_addr  sin_addr;       //IPv4 address
   *   };
   *
   *  -->socklen_t
   *  socklen_t describes the length of a socket address.
   *  This is an integer type of at least 32 bits.
   */
  socklen_t client_addr_size = sizeof(clientaddr);
  socklen_t server_addr_size = sizeof(myaddr);
  serverfd = socket(AF_INET, SOCK_STREAM, 0);
  /*
   * socket() creates an endpoint for communication
   * and returns a file descriptor that refers to
   * that endpoint.
   * On success, a file descriptor for the new socket is returned.
   * On error, -1 is returned, and errno is set to indicate the error.
   */
  if (serverfd == -1) {
    // perror - print a system error message
    perror("Error : Socket has not been created");
    return -1;
  }

  // memset() devuelve un puntero al área de memoria myaddr.
  memset(&myaddr, 0, sizeof(myaddr));
  // Creo la estructura
  myaddr.sin_addr.s_addr = INADDR_ANY;
  myaddr.sin_port = htons(SERVER_PORT);
  myaddr.sin_family = AF_INET;

  /*
   * When  a  socket is created with socket(2), it exists in a name space
   * (address family) but has no address assigned to it. bind() assigns the
   * address specified by addr to the socket referred to by the file descriptor
   * serverfd.
   */
  init_connection(serverfd, &myaddr, server_addr_size);
  return serverfd;
}

// It could be interpreted as the communication from client to user in form of message
char *init_message(int clientfd, char *buffer) {
  /*
   * The  recv()  call is normally used only on a connected socket (see
   * connect(2)).  It is equivalent to the call:
   * recvfrom(fd, buf, size, flags, NULL, NULL);
   *
   * The  recv(),  recvfrom(),  and  recvmsg() calls are used to receive
   * messages from a socket.  They may be used to receive data on both
   * connectionless and connection-oriented sockets.  This page first
   * describes common features of all three system calls, and then describes
   * the differences between the calls.
   *
   * The only  difference  between recv() and read(2) is the presence of
   * flags. With a zero flags argument, recv() is generally equivalent to
   * read(2)
   **/

  int bytes_received = recv(clientfd, buffer, 1023, 0);
  if (bytes_received <= 0) {
    perror("Error : Could not receive data");
    return NULL;
  }

  buffer[bytes_received] = '\0';
  return buffer;
}

int main() {
  struct sockaddr_in clientaddr;
  socklen_t client_addr_size = sizeof(clientaddr); // size of previous address
  int serverfd = init_server(); // already initialized
  int clientfd;
  // The server initialization has FAILED
  if (serverfd == -1) {
    return -1;
  }
  printf("Escuchando por el puerto %d...\n", SERVER_PORT);


  while (1) {
    /*
     * The  accept()  system call is used with connection-based socket types
     * (SOCK_STREAM, SOCK_SEQPACKET).  It extracts the first connection request
     * on the queue of pending  connections  for  the  listening  socket,
     * sockfd, creates a new connected socket, and returns a new file descriptor
     * referring to that socket.  The newly  created  socket  is not in the
     * listening state.
     */
    clientfd =
        accept(serverfd, (struct sockaddr *)&clientaddr, &client_addr_size);
    if (clientfd == -1) {
      perror("Error : Could not accept");
      continue;
    }

    char buffer[1024];
    // message from the first communications between the two
    char *mensaje = init_message(clientfd, buffer);
    // communication has FAILED
    if (mensaje == NULL) {
      close(clientfd);
      continue;
    }
    printf("received: %s\n", mensaje);

    
    // CORRECTLY FORMATTED!
    char *response = "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: 39\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "The cake is a lie. But the server is ";
    /*
     * The system calls send(), sendto(), and sendmsg() are used to transmit a
     * message to another socket.
     *
     * The send() call may be used only when the socket is in a connected state
     * (so that the intended recipient is known).  The only difference between
     * send() and write(2) is the presence of flags.  With a zero flags
     * argument, send() is equivalent to write(2).  Also, the following call
     *
     * send(sockfd, buf, size, flags);
     * is equivalent to:
     * sendto(sockfd, buf, size, flags, NULL, 0);
     * The argument sockfd is the file descriptor of the sending socket.
     */
     
    // now we send our own response for better comunication
    send(clientfd, response, strlen(response), 0);
    close(clientfd);
  }
  close(serverfd);
  return 0;
}
