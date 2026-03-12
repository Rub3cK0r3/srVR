#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Custom port number
#define SERVER_PORT 8080

// ANSI colors to make it look cooler
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define RESET   "\033[0m"

//Prototypes for functions I used
int init_connection(int serverfd, struct sockaddr_in *myaddr,socklen_t addr_size);
int init_server();
char *init_message(int clientfd,char *buffer);
