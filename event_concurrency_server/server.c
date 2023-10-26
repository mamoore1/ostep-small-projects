#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>


#define PORT 7890  // The port users will be connecting to


void dump(const unsigned char *, const unsigned int);
void fatal(char *);

/* Base server code taken from Hacking: The Art of Exploitation - Erickson*/
int main(void) {
    int sockfd, new_sockfd;  // Listen on sock_fd, new_connection on new_fd
    struct sockaddr_in host_addr, client_addr;  // My address information
    socklen_t sin_size;
    int recv_length=1, yes=1;
    char request_buffer[1024];
    char response_buffer[1024];

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        fatal("in socket");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        fatal("setting socket option SO_REUSEADDR");

    host_addr.sin_family = AF_INET;  // Host byte order
    host_addr.sin_port = htons(PORT);  // Short, network byte order
    host_addr.sin_addr.s_addr = 0;  // Automatically fill with my IP
    memset(&(host_addr.sin_zero), '\0', 8);  // Zero the rest of the struct

    if (bind(sockfd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr)) == -1)
        fatal("binding to socket");

    if (listen(sockfd, 5) == -1)
        fatal("listening on socket");

    struct timeval tv;

    while (1) {  // Accept loop
        sin_size = sizeof(struct sockaddr_in);
        new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_sockfd == -1)
            fatal("accepting connection");
        printf(
            "server: got connection from %s port %d\n",
            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)
        );

        gettimeofday(&tv, NULL);
        sprintf(response_buffer, "%ld:%ld\n", tv.tv_sec, tv.tv_usec);
        send(new_sockfd, response_buffer, strlen(response_buffer), 0);
        close(new_sockfd);
    }
    return 0;
}


/* H:TAoE */
void fatal(char *message) {
    char error_message[100];

    strcpy(error_message, "[!!] Fatal Error ");
    strncat(error_message, message, 83);
    perror(error_message);
    exit(-1);
}
