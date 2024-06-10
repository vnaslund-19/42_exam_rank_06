#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>

typedef struct s_client 
{
    int id;
    char* buff;
} t_client;

typedef struct s_server
{
    int fd;
    struct sockaddr_in address;
    int clients_count;
    t_client clients[1024];
    int max_fd;
    fd_set conn_set;
    fd_set read_set;
    fd_set write_set;
} t_server;

t_server server;

void error_exit(const char *message) 
{
    if (message)
        write(2, message, strlen(message));
    exit(1);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void broadcast_message(int sender_fd, const char *message) 
{
    for (int i = 0; i <= server.max_fd; ++i) 
    {
        if (FD_ISSET(i, &server.write_set) && i != sender_fd && i != server.fd)
            send(i, message, strlen(message), 0);
    }
}

void init_server(const char *port)
{
    server.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.fd == -1)
        error_exit("Fatal error\n");

    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server.address.sin_port = htons(atoi(port));

    if (bind(server.fd, (const struct sockaddr*)&server.address, sizeof(server.address)) == -1)
        error_exit("Fatal error\n");

    if (listen(server.fd, 100) == -1)
        error_exit("Fatal error\n");

    server.clients_count = 0;
    server.max_fd = server.fd;
    FD_ZERO(&server.conn_set);
    FD_SET(server.fd, &server.conn_set);

    for (int i = 0; i < 1024; ++i)
        server.clients[i].buff = NULL;
}

void handle_new_client()
{
    int client_fd = accept(server.fd, NULL, NULL);
    if (client_fd == -1)
        return;

    server.clients[client_fd].id = server.clients_count++;
    server.clients[client_fd].buff = NULL;
    if (client_fd > server.max_fd)
        server.max_fd = client_fd;

    FD_SET(client_fd, &server.conn_set);
    char message[128];
    sprintf(message, "server: client %d just arrived\n", server.clients[client_fd].id);
    broadcast_message(client_fd, message);
}

void handle_client_disconnection(int client_fd)
{
    char message[128];
    sprintf(message, "server: client %d just left\n", server.clients[client_fd].id);
    broadcast_message(client_fd, message);

    FD_CLR(client_fd, &server.conn_set);
    FD_CLR(client_fd, &server.write_set);
    close(client_fd);
    free(server.clients[client_fd].buff);
    server.clients[client_fd].buff = NULL;
}

void save_remaining_message(int client_fd, int bytes, int start)
{
    char *temp = NULL;

    if (start != bytes) 
    {
        temp = str_join(temp, &server.clients[client_fd].buff[start]);
        if (!temp)
            error_exit("Fatal error\n");
    }
    free(server.clients[client_fd].buff);
    server.clients[client_fd].buff = temp;
}

void process_client_message(int client_fd, int bytes, char* buffer)
{
    int start = 0;
    for (int i = 0; i < bytes; ++i) 
    {
        if (buffer[i] == '\n') 
        {
            buffer[i] = '\0';
            char *line = malloc(i - start + 32);
            if (!line)
                error_exit("Fatal error\n");
            sprintf(line, "client %d: %s\n", server.clients[client_fd].id, &buffer[start]);
            broadcast_message(client_fd, line);
            free(line);
            start = i + 1;
        }
    }
    save_remaining_message(client_fd, bytes, start);
}

void handle_client_data(int client_fd)
{
    char buffer[70000];
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) 
    {
        handle_client_disconnection(client_fd);
        return;
    }

    buffer[bytes] = '\0';
    server.clients[client_fd].buff = str_join(server.clients[client_fd].buff, buffer);
    if (!server.clients[client_fd].buff)
        error_exit("Fatal error\n");

    process_client_message(client_fd, strlen(server.clients[client_fd].buff), server.clients[client_fd].buff);
}

int main(int argc, char **argv) 
{
    if (argc != 2)
        error_exit("Wrong number of arguments\n");

    init_server(argv[1]);

    while (1) 
    {
        server.read_set = server.conn_set;
        server.write_set = server.conn_set;
        if (select(server.max_fd + 1, &server.read_set, &server.write_set, NULL, NULL) == -1) 
        {
            if (errno != EINTR)
                error_exit("Fatal error\n");
            continue;
        }

        for (int i = 0; i <= server.max_fd; ++i)
        {
            if (FD_ISSET(i, &server.read_set)) 
            {
                if (i == server.fd)
                    handle_new_client();
                else
                    handle_client_data(i);
            }
        }
    }

    return 0;
}
