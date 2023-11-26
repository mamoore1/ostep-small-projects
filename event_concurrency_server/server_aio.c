#include <fcntl.h>
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
#include <string.h>
#include <aio.h>
#include <errno.h>
#include <signal.h>

#include "helpers.h"

#define PORT 7890  // The port users will be connecting to
#define BUF_SIZE 1024

#define IO_SIGNAL SIGUSR1 /* Signal used to notify I/O completion */

struct ioRequest {
    int socket_descriptor;
    int reqNum;
    int status;
    struct aiocb *aiocbp;
};


/* Base server code taken from Hacking: The Art of Exploitation - Erickson*/
int main(void) {
    int server_fd, new_sockfd, i, max_sd, sd, activity;  // Listen on sock_fd, new_connection on new_fd
    struct sockaddr_in host_addr, client_addr;  // My address information
    socklen_t sin_size;
    int yes=1;
    
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

    struct ioRequest *ioList = calloc(max_clients, sizeof(struct ioRequest));
    struct aiocb *aiocbList = calloc(max_clients, sizeof(struct aiocb));
    int iorequest_index, request_count = 0;

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

        // Check for activity once a second
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        activity = select(max_sd + 1, &readFDs, NULL, NULL, &timeout);

        if (activity < 0) {
            printf("Select failed.");
        } 

        // If there's activity on the server file descriptor, accept the incoming connection
        if (FD_ISSET(server_fd, &readFDs)) {
            sin_size = sizeof(struct sockaddr_in);
            if ((new_sockfd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size)) < 0) {
                printf("Failure to accept connection\n");
            }
            else {
                printf(
                    "server: got connection from %s port %d\n",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)
                );

                // Send a list of files
                char directory_buffer[BUF_SIZE];
                get_openable_file_list(".", directory_buffer);
                
                send(new_sockfd, directory_buffer, strlen(directory_buffer), 0);
                send(new_sockfd, "\nEnter the name of a file to open:\n", 35, 0);
            }
        
            for (i = 0; i < max_clients; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_sockfd;
                    break;
                }
            }
        }
        else {
            for (i = 0; i < max_clients; i++) {
                sd = client_socket[i];
                if (FD_ISSET(sd, &readFDs)) {
                    // This handles the read request
                    int recv_length, s;
                    char request_buffer[BUF_SIZE];
                    
                    recv_length = recv(sd, &request_buffer, BUF_SIZE, 0);
                    if (recv_length == 0) {
                        // Client has disconnected gracefully
                        printf("Client on socket %d disconnected\n", sd);
                        close(sd);
                        client_socket[i] = 0; // Remove from client sockets list
                    } else if (recv_length == -1) {
                        // Error occurred in receiving
                        printf("recv failed");
                        close(sd);
                        client_socket[i] = 0;
                    } else {
                        char* ptr = strchr(request_buffer, '\n');
                        if (ptr) {
                            if (ptr > request_buffer && *(ptr - 1) == '\r') {
                                *(ptr - 1) = '\0';
                            }
                            *ptr = '\0';
                        }
                        printf("Attempting to open file: %s\n", request_buffer);

                        // Check that the file is a) valid for reading
                        // (i.e., is a .txt file), and b) is in the current directory
                        int filedes;
                        char directory_buffer[1024];
                        get_openable_file_list(".", directory_buffer);

                        if (endswith(request_buffer, ".txt") != 0) {
                            printf("Failed to open file with invalid name '%s'\n", request_buffer);
                            send(sd, "Invalid filename, please enter one of the listed filenames.\n", 61, 0);
                            continue;
                        } else if (strstr(directory_buffer, request_buffer) == NULL) {
                            printf("Failed to open file %s, not found in current directory.\n", request_buffer);
                            send(sd, "Invalid filename, please enter one of the listed filenames.\n", 61, 0);
                            continue;
                        } else if ((filedes = open(request_buffer, O_RDONLY)) < 0) {
                            printf("Failed to open file %s\n", request_buffer);
                            send(sd, "Failed to open file, please try again.\n", 40, 0);
                            continue;
                        } else {
                            // Create an ioRequest and an aiocb
                            iorequest_index = request_count % max_clients;

                            ioList[iorequest_index].socket_descriptor = sd;
                            ioList[iorequest_index].reqNum = request_count + 1;
                            ioList[iorequest_index].status = EINPROGRESS;
                            ioList[iorequest_index].aiocbp = &aiocbList[iorequest_index];
                            ioList[iorequest_index].aiocbp->aio_fildes = filedes; 
                            ioList[iorequest_index].aiocbp->aio_buf = malloc(BUF_SIZE);
                            ioList[iorequest_index].aiocbp->aio_nbytes = BUF_SIZE;
                            ioList[iorequest_index].aiocbp->aio_reqprio = 0;
                            ioList[iorequest_index].aiocbp->aio_offset = 0;
                            // ioList[iorequest_index].aiocbp->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
                            // VSCode complains there's no `sival_ptr` and suggests `__sival_ptr`, but gcc says to use `sival_ptr`?
                            ioList[iorequest_index].aiocbp->aio_sigevent.sigev_value.sival_ptr = &ioList[iorequest_index];                
                            
                            printf("opened %s on descriptor %d\n", request_buffer, ioList[iorequest_index].aiocbp->aio_fildes);
                            // Add this back in to use a signal to handle IO finishing
                            // ioList[iorequest_index].aiocbp->aio_sigevent.sigev_signo = IO_SIGNAL;

                            s = aio_read(ioList[iorequest_index].aiocbp);
                            if (s == -1) {
                                fatal("aio_read");
                            } 
                            ioList[iorequest_index].status = EINPROGRESS;
                            request_count++;
                        }
                    }
                }
            }
        }

        // Check whether any of the async read requests have returned
        for (int i = 0; i < max_clients; i++) {
            if (ioList[i].status == EINPROGRESS) {
                printf("Request %d on descriptor %d\n", ioList[i].reqNum, ioList[i].aiocbp->aio_fildes);
                ioList[i].status = aio_error(ioList[i].aiocbp);

                switch (ioList[i].status) {
                        case 0: {
                        printf("I/O succeeded\n");
                        // Handle the I/O
                        // In other words, accept the contents of the read and return the information back to the
                        // awaiting socket

                        // Add an extra newline to the end of the buffer for formatting purposes
                        char output_buffer[BUF_SIZE + 1];
                        strcpy(output_buffer, (char *) ioList[i].aiocbp->aio_buf);
                        int output_buffer_final_index = strlen(output_buffer);
                        output_buffer[output_buffer_final_index] = '\n';
                        output_buffer[output_buffer_final_index + 1] = '\0';

                        send(ioList[i].socket_descriptor, output_buffer, strlen(output_buffer), 0);
                        send(ioList[i].socket_descriptor, "Enter the name of a file to open:\n", 35, 0);

                        // Close the file and cleanup the aoicb and ioList records
                        close(ioList[i].aiocbp->aio_fildes);
                        memset(ioList[i].aiocbp, 0, sizeof(struct aiocb));
                        memset(ioList + i, 0, sizeof(struct ioRequest));
                        break;
                    }
                    case EINPROGRESS: {
                        printf("In progress\n");
                        break;
                    }
                    case ECANCELED: {
                        printf("Canceled\n");
                        break;
                    }
                    default: {
                        fatal("aio_error");
                        break;
                    }
                }
            }
        }
    }    
}
