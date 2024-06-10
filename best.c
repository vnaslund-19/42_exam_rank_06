#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

typedef struct s_client 
{
    int id;
    char* buff;
} t_client;

int server_fd;                      // File descriptor for the server socket
struct sockaddr_in server_address; // Struct for server address information
int clients_count;                 // Counter for clients
t_client clients[1024];            // Array to store client information
int max_fd;                        // Maximum file descriptor number
fd_set conn_set; // Set of all connected sockets
fd_set read_set; // Set of sockets ready to be read
fd_set write_set; // Set of sockets ready to be written to

void    err(char  *msg)
{
    if (msg)
        write(2, msg, strlen(msg));
    else
        write(2, "Fatal error", 11);
    write(2, "\n", 1);
    exit(1);
}

char *str_join(char *buf, char *add)
{
    char *newbuf;
    int len;

    if (buf == 0)
        len = 0;
    else
        len = strlen(buf);
    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
    if (newbuf == 0)
        return 0;
    newbuf[0] = 0;
    if (buf != 0)
        strcat(newbuf, buf);
    free(buf);
    strcat(newbuf, add);
    return newbuf;
}

void broadcast_message(int sender_fd, const char *message) 
{
    for (int i = 0; i <= max_fd; ++i) 
    {
        if (FD_ISSET(i, &write_set) && i != sender_fd && i != server_fd)
            send(i, message, strlen(message), 0);
    }
}

void handle_new_client()
{
    int client_fd = accept(server_fd, NULL, NULL); // Accept new client connection
    if (client_fd == -1)
        return;

    // Assign an ID to the new client
    clients[client_fd].id = clients_count++;
    clients[client_fd].buff = NULL;
    if (client_fd > max_fd)
        max_fd = client_fd;

    FD_SET(client_fd, &conn_set); // Add the new client socket to the set
    char message[128];
    sprintf(message, "server: client %d just arrived\n", clients[client_fd].id);
    broadcast_message(client_fd, message); // Notify other clients about the new client
}

void handle_client_disconnection(int client_fd)
{
    char message[128];
    sprintf(message, "server: client %d just left\n", clients[client_fd].id);
    broadcast_message(client_fd, message); // Notify other clients about the disconnection

    FD_CLR(client_fd, &conn_set);   // Remove the client from the sets
    FD_CLR(client_fd, &write_set);
    close(client_fd);               // Close the client socket
    free(clients[client_fd].buff); // Free the client's buffer
    clients[client_fd].buff = NULL;
}

void save_remaining_message(int client_fd, int bytes, int start)
{
    char *temp = NULL;

    if (start != bytes) 
    {
        temp = str_join(temp, &clients[client_fd].buff[start]);
        if (!temp)
            err(NULL);
    }
    free(clients[client_fd].buff);
    clients[client_fd].buff = temp; // Save any remaining part of the message
}

void process_client_message(int client_fd, int bytes, char* buffer)
{
    int start = 0;
    for (int i = 0; i < bytes; ++i) 
    {
        if (buffer[i] == '\n') 
        {
            buffer[i] = '\0'; // Replace newline with null terminator
            char *line = malloc(i - start + 32); // Allocate memory for the message line
            if (!line)
                err(NULL);
            sprintf(line, "client %d: %s\n", clients[client_fd].id, &buffer[start]);
            broadcast_message(client_fd, line); // Send the message line to other clients
            free(line);
            start = i + 1;
        }
    }
    save_remaining_message(client_fd, bytes, start); // Save any remaining part of the message
}

void handle_client_data(int client_fd)
{
    char buffer[70000];
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0); // Receive data from the client

    if (bytes <= 0) 
    {
        handle_client_disconnection(client_fd); // Handle client disconnection if no data is received
        return;
    }

    buffer[bytes] = '\0'; // Null-terminate the received data
    clients[client_fd].buff = str_join(clients[client_fd].buff, buffer); // Append the new data to the client's buffer
    if (!clients[client_fd].buff)
        err(NULL);

    process_client_message(client_fd, strlen(clients[client_fd].buff), clients[client_fd].buff); // Process the received message
}

void init_server(int port)
{
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        err(NULL);

    // Set up server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_address.sin_port = htons(port);

    // Bind socket to the address
    if (bind(server_fd, (const struct sockaddr*)&server_address, sizeof(server_address)) == -1)
        err(NULL);

    // Listen for connections
    if (listen(server_fd, 100) == -1) // 100 is maximum lenght for queue of pending connections
        err(NULL);

    clients_count = 0;
    max_fd = server_fd;
    FD_ZERO(&conn_set); // Initialize
    FD_SET(server_fd, &conn_set); // Add the server socket to the set

    for (int i = 0; i < 1024; ++i)
        clients[i].buff = NULL; // Initialize client buffers to NULL
}

int main(int argc, char **argv) 
{
    if (argc != 2)
        err("Wrong number of arguments\n");

    init_server(atoi(argv[1]));

    while (1) 
    {
        read_set = conn_set;
        write_set = conn_set;
        if (select(max_fd + 1, &read_set, &write_set, NULL, NULL) == -1) 
            continue;

        for (int i = 0; i <= max_fd; ++i)
        {
            if (FD_ISSET(i, &read_set)) 
            {
                if (i == server_fd)
                    handle_new_client();
                else
                    handle_client_data(i);
            }
        }
    }
    return 0;
}
