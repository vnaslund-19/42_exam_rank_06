#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client 
{
    int id;
    char* buff;
} t_client;

int server_fd;                    // File descriptor for the server socket
struct sockaddr_in servaddr;    // Struct for server address information
int clients_count = 0;            // Counter for clients
t_client clients[1024];           // Array to store client information
int max_fd;                       // Maximum file descriptor number
fd_set conn_set; // Set of all connected sockets
fd_set read_set; // Set of sockets ready to be read
fd_set write_set; // Set of sockets ready to be written to

void    err(char  *msg)
{
    if (msg)
        write(2, msg, strlen(msg));
    else
        write(2, "Fatal error\n", 12);
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

void broadcast_message(int sender_fd, const char *msg) 
{
    for (int i = 0; i <= max_fd; ++i) 
    {
        if (FD_ISSET(i, &write_set) && i != sender_fd && i != server_fd)
            send(i, msg, strlen(msg), 0);
    }
}

void handle_new_client()
{
    int client_fd = accept(server_fd, NULL, NULL); // Accept new client connection
    if (client_fd == -1)
        return;

    // Assign an ID to the new client
    clients[client_fd].id = clients_count++;
    if (client_fd > max_fd)
        max_fd = client_fd;

    FD_SET(client_fd, &conn_set); // Add the new client socket to the set
    char msg[128];
    sprintf(msg, "server: client %d just arrived\n", clients[client_fd].id);
    broadcast_message(client_fd, msg); // Notify other clients about the new client
}

void disconnect_client(int client_fd)
{
    char msg[128];
    sprintf(msg, "server: client %d just left\n", clients[client_fd].id);
    broadcast_message(client_fd, msg); // Notify other clients about the disconnection

    FD_CLR(client_fd, &conn_set);   // Remove the client from the sets
    FD_CLR(client_fd, &write_set);
    close(client_fd);               // Close the client socket
    free(clients[client_fd].buff); // Free the client's buffer
    clients[client_fd].buff = NULL;
}

void save_remaining_msg(int client_fd, int bytes, int start)
{
    char *temp = NULL;

    if (start != bytes) 
    {
        temp = str_join(temp, &clients[client_fd].buff[start]);
        if (!temp)
            err(NULL);
    }
    free(clients[client_fd].buff);
    clients[client_fd].buff = temp; // Save any remaining part of the msg
}

void process_client_msg(int client_fd, int bytes, char* buffer)
{
    int start = 0;
    for (int i = 0; i < bytes; ++i) 
    {
        if (buffer[i] == '\n') 
        {
            buffer[i] = '\0'; // Replace newline with null terminator
            char *line = malloc(i - start + 32); // Allocate memory for the msg line
            if (!line)
                err(NULL);
            sprintf(line, "client %d: %s\n", clients[client_fd].id, &buffer[start]);
            broadcast_message(client_fd, line); // Send the msg line to other clients
            free(line);
            start = i + 1;
        }
    }
    save_remaining_msg(client_fd, bytes, start); // Save any remaining part of the msg
}

void handle_client_data(int client_fd)
{
    char buffer[70000];
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0); // Receive data from the client

    if (bytes <= 0) 
    {
        disconnect_client(client_fd); // Handle client disconnection if no data is received
        return;
    }

    buffer[bytes] = '\0'; // Null-terminate the received data
    clients[client_fd].buff = str_join(clients[client_fd].buff, buffer); // Append the new data to the client's buffer
    if (!clients[client_fd].buff)
        err(NULL);

    process_client_msg(client_fd, strlen(clients[client_fd].buff), clients[client_fd].buff); // Process the received msg
}

void init_server(int port)
{
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        err(NULL);

    // Set up server address
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port = htons(port);

    // Bind socket to the address
    if (bind(server_fd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
        err(NULL);

    // Listen for connections
    if (listen(server_fd, 100) != 0) // 100 is maximum lenght for queue of pending connections
        err(NULL);

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
