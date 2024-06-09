#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Structure to store client information
typedef struct s_client
{
    int     id;         // Client ID
    char    msg[100000]; // Buffer to store client messages
}   t_client;

t_client    clients[1024];        // Array to store all connected clients
fd_set      read_set, write_set, current; // Sets of file descriptors for select
int         maxfd = 0, gid = 0;   // maxfd: highest file descriptor number, gid: global client ID counter
char        send_buffer[120000], recv_buffer[120000]; // Buffers for sending and receiving data

// Function to handle errors by printing a message and exiting the program
void    err(char  *msg)
{
    if (msg)
        write(2, msg, strlen(msg));
    else
        write(2, "Fatal error", 11);
    write(2, "\n", 1);
    exit(1);
}

// Function to send messages to all clients except the one specified by 'except'
void    send_to_all(int except)
{
    for (int fd = 0; fd <= maxfd; fd++)
    {
        if (FD_ISSET(fd, &write_set) && fd != except) // Checks if the fd is part of the write_set
            if (send(fd, send_buffer, strlen(send_buffer), 0) == -1)
                err(NULL);
    }
}

// Main function
int     main(int ac, char **av)
{
    if (ac != 2) // Only port number should be passed as arg
        err("Wrong number of arguments");

    struct      sockaddr_in  serveraddr; // struct holding address information of the socket
    socklen_t   len;

    int serverfd = socket(AF_INET, SOCK_STREAM, 0); // Create a socket, Address family: IPv4, Type of socket: TCP
    if (serverfd == -1) 
        err(NULL);
    maxfd = serverfd;

    FD_ZERO(&current); // Initialize the file descriptor set
    FD_SET(serverfd, &current); // Add the server socket to the set
    bzero(clients, sizeof(clients)); // Clear the clients array
    bzero(&serveraddr, sizeof(serveraddr)); // Clear the server address structure

    serveraddr.sin_family = AF_INET; // Set the address family to AF_INET
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // Set the address to any available interface (host to network long)
    serveraddr.sin_port = htons(atoi(av[1])); // Set the port number from the argument (host to network short)

    // Bind and listen on the socket
    if (bind(serverfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1 || listen(serverfd, 100) == -1)
        err(NULL);                                                              // 100 is maximum lenght for queue of pending connections

    while (1) // Main loop to handle client connections and messages
    {
        read_set = write_set = current; // Copy the current set to read_set and write_set
        if (select(maxfd + 1, &read_set, &write_set, 0, 0) == -1) // Wait for activity on the sockets
            continue;

        for (int fd = 0; fd <= maxfd; fd++) // Iterate through file descriptors
        {
            if (FD_ISSET(fd, &read_set)) // Check if the file descriptor is ready for reading
            {
                if (fd == serverfd) // If the server socket is ready, accept a new client connection
                {
                    int clientfd = accept(serverfd, (struct sockaddr *)&serveraddr, &len);
                    if (clientfd == -1)
                        continue;
                    if (clientfd > maxfd)
                        maxfd = clientfd; // Update maxfd if necessary
                    clients[clientfd].id = gid++; // Assign a unique ID to the new client
                    FD_SET(clientfd, &current); // Add the new client socket to the current set
                    sprintf(send_buffer, "server: client %d just arrived\n", clients[clientfd].id);
                    send_to_all(clientfd); // Notify all clients about the new connection
                    break;
                }
                else // If a client socket is ready, receive data from the client
                {
                    int ret = recv(fd, recv_buffer, sizeof(recv_buffer), 0);
                    if (ret <= 0) // If recv returns <= 0, the client has disconnected
                    {
                        sprintf(send_buffer, "server: client %d just left\n", clients[fd].id);
                        send_to_all(fd); // Notify all clients about the disconnection
                        FD_CLR(fd, &current); // Remove the client socket from the current set
                        close(fd); // Close the client socket
                        break;
                    }
                    else // Process the received data
                    {
                        for (int i = 0, j = strlen(clients[fd].msg); i < ret; i++, j++)
                        {
                            clients[fd].msg[j] = recv_buffer[i]; // Append received data to the client's message buffer
                            if (clients[fd].msg[j] == '\n') // Check for complete messages ending with '\n'
                            {
                                clients[fd].msg[j] = '\0'; // Null-terminate the message
                                sprintf(send_buffer, "client %d: %s\n", clients[fd].id, clients[fd].msg);
                                send_to_all(fd); // Send the complete message to all clients
                                bzero(clients[fd].msg, strlen(clients[fd].msg)); // Clear the client's message buffer
                                j = -1; // Reset the buffer index
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    return (0);
}

