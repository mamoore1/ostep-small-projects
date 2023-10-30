#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>


#define PORT 7890  // The port users will be connecting to


void dump(const unsigned char *, const unsigned int);
void fatal(char *);
void handle_connection(int , struct sockaddr_in *);

/* Base server code taken from Hacking: The Art of Exploitation - Erickson*/
int main(void) {
    int server_fd, new_sockfd, i, max_sd, sd, activity;  // Listen on sock_fd, new_connection on new_fd
    struct sockaddr_in host_addr, client_addr;  // My address information
    socklen_t sin_size;
    int recv_length=1, yes=1;
    char request_buffer[1024];
    
    if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        fatal("in socket");

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        fatal("setting socket option SO_REUSEADDR");

    host_addr.sin_family = AF_INET;  // Host byte order
    host_addr.sin_port = htons(PORT);  // Short, network byte order
    host_addr.sin_addr.s_addr = 0;  // Automatically fill with my IP
    memset(&(host_addr.sin_zero), '\0', 8);  // Zero the rest of the struct

    if (bind(server_fd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr)) == -1)
        fatal("binding to socket");

    if (listen(server_fd, 5) == -1)
        fatal("listening on socket");


    int client_socket[10], max_clients=10;

    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }

    while (1) {
        fd_set readFDs;
        FD_ZERO(&readFDs);

        FD_SET(server_fd, &readFDs);
        max_sd = server_fd;

        int i;
        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];

            if (sd > 0) {
                FD_SET(sd, &readFDs);
            }

            if (sd > max_sd) {
                max_sd = sd;
            }

        }

        activity = select(max_sd + 1, &readFDs, NULL, NULL, NULL);

        if (activity < 0) {
            printf("Select failed.");
        } else {
            printf("Activity detected. Number of ready descriptors: %d\n", activity);
            for (int i = 0; i <= max_sd; i++) {
                if (FD_ISSET(i, &readFDs)) {
                    printf("Descriptor %d is ready for reading\n", i);
                }
            }
        }

        if (FD_ISSET(server_fd, &readFDs)) {
            sin_size = sizeof(struct sockaddr_in);
            if ((new_sockfd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size)) < 0) {
                fatal("accepting connection");
            }
        
            for (i = 0; i < max_clients; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_sockfd;
                    break;
                }
            }
        }

        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];
            if (FD_ISSET(sd, &readFDs)) {
                printf("handling %d\n", sd);
                handle_connection(sd, &client_addr);
                client_socket[i] = 0;
            }
        }
    }
    
    return 0;
}


void handle_connection(int sockfd, struct sockaddr_in *client_addr) {
    char response_buffer[1024];
    struct timeval tv;

    printf(
        "server: got connection from %s port %d\n",
        inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port)
    );

    gettimeofday(&tv, NULL);
    sprintf(response_buffer, "%ld:%ld\n", tv.tv_sec, tv.tv_usec);
    send(sockfd, response_buffer, strlen(response_buffer), 0);
}



/* H:TAoE */
void fatal(char *message) {
    char error_message[100];

    strcpy(error_message, "[!!] Fatal Error ");
    strncat(error_message, message, 83);
    perror(error_message);
    exit(-1);
}
