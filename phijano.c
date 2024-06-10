#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

typedef struct s_client
{
	int		id;
	char*	buff;

}	t_client;

typedef struct s_serv
{
	int					fd;
	struct sockaddr_in	adress;
	int					clients_number;
	t_client			clients[1024];
	int					max_fd;
	fd_set				conn;
	fd_set				read_set;
	fd_set				write_set;

}	t_serv;

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

void	ft_exit_error(t_serv *serv, char *message)
{
	int client;

	if (message)
		write(2, message, strlen(message));
	exit(1);

	client = -1;
	if (serv)
		while(++client < 1024)
			if (serv->clients[client].buff)
				free(serv->clients[client].buff);
}

void	ft_send_others(t_serv *serv, int fd, char *message)
{
	int	i;

	i = 2;
	while (++i <= serv->max_fd)
	{
		if (FD_ISSET(i, &(serv->write_set)) && i != fd && i != serv->fd)
			send(i, message, strlen(message), 0);
	}
}

void	ft_init_serv(t_serv *serv, char *port)
{
	int client;

	serv->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (serv->fd == -1)
		ft_exit_error(NULL, "Fatal error\n");
	serv->adress.sin_family = AF_INET;
	serv->adress.sin_addr.s_addr = htonl(0x7f000001);
	serv->adress.sin_port = htons(atoi(port));
	if (bind(serv->fd, (const struct sockaddr*)&(serv->adress), sizeof(serv->adress)) == -1)
		ft_exit_error(NULL, "Fatal error\n");
	if (listen(serv->fd, 100)== -1)
		ft_exit_error(NULL, "Fatal error\n");
	serv->clients_number = 0;
	serv->max_fd = serv->fd;
	FD_ZERO(&(serv->conn));
	FD_SET(serv->fd, &(serv->conn));
	client = -1;
	while (++client < 1024)
		serv->clients[client].buff = NULL;
}

void ft_new_client(t_serv *serv)
{
	int		client;
	char	message[128];

	client = accept(serv->fd, NULL, NULL);
	if (client == -1)
		return ;
	serv->clients[client].id = serv->clients_number++;
	serv->clients[client].buff = NULL;
	if (client > serv->max_fd)
		serv->max_fd = client;
	FD_SET(client, &(serv->conn));
	sprintf(message, "server: client %d just arrived\n", serv->clients[client].id);
	ft_send_others(serv, client, message);
}

void ft_close_client(t_serv *serv, int client)
{
	char message[128];

	sprintf(message, "server: client %d just left\n", serv->clients[client].id);
	ft_send_others(serv, client, message);
	FD_CLR(client, (&serv->conn));
	FD_CLR(client, (&serv->write_set));
	close(client);
	if (serv->clients[client].buff)
		free(serv->clients[client].buff);
	serv->clients[client].buff = NULL;
}

void ft_save_no_line(t_serv *serv, int client, int bytes, int start)
{
	char *temp = NULL;

	if (start == bytes)
	{
		free(serv->clients[client].buff);
		serv->clients[client].buff = NULL;
	}
	else
	{
		temp = str_join(temp, &(serv->clients[client].buff[start]));
		if (!temp)	
			ft_exit_error(serv, "Fatal error\n");
		free(serv->clients[client].buff);
		serv->clients[client].buff = temp;
	}
}

void ft_write_message(t_serv *serv, int client, int bytes, char* buffer)
{
	char	*line;
	int		start;
	int		buf_index;

	start = 0;
	buf_index = -1;
	while (++buf_index < bytes)
	{
		if(buffer[buf_index] == '\n')
		{
			line = malloc(sizeof(*line) * (buf_index - start) + 32);
			if (!line)
				ft_exit_error(serv, "Fatal error\n");
			buffer[buf_index] = '\0';
			sprintf(line, "client %d: %s\n", serv->clients[client].id, &buffer[start]);
			ft_send_others(serv, client, line);
			free(line);
			start = buf_index + 1;
		}
	}
	ft_save_no_line(serv, client, bytes, start);
}

void ft_client_data(t_serv *serv, int client)
{
	char	read_set_buff[70000];
	int 	bytes = 0;

	bytes = recv(client, read_set_buff, sizeof(read_set_buff) - 1, 0);
	if (bytes > 0)
	{
		read_set_buff[bytes] = '\0';
		serv->clients[client].buff = str_join(serv->clients[client].buff, read_set_buff);
		if (!serv->clients[client].buff)
			ft_exit_error(serv, "Fatal error\n");
		ft_write_message(serv, client, strlen(serv->clients[client].buff), serv->clients[client].buff);
	}
	else
		ft_close_client(serv, client);
}

int	main(int argc, char **args)
{
	t_serv server;
	int		i;

	if (argc != 2)
		ft_exit_error(NULL, "Wrong number of arguments\n");
	ft_init_serv(&server, args[1]);
	while(1)
	{
		server.read_set = server.conn;
		server.write_set = server.conn;
		if (select(server.max_fd + 1, &(server.read_set), &(server.write_set), NULL, NULL) == -1)
			continue ;
		i = 2;
		while (++i <= server.max_fd)
		{
			if (FD_ISSET(i, &(server.read_set)))
			{
				if (i == server.fd)
					ft_new_client(&server);
				else
					ft_client_data(&server, i);
			}
		}
	}
	return (0);
}
