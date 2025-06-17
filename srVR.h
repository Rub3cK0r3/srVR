#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Custom port number
#define SERVER_PORT 8080

//Prototypes for functions I used
int init_connection(int serverfd, struct sockaddr_in *myaddr,socklen_t addr_size);
int init_server();
char *init_message(int clientfd,char *buffer);
