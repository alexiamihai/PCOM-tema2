#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/select.h>

#define PACKET_SIZE sizeof(struct packet)
#define TCP_SIZE sizeof(struct tcp_struct)
#define UDP_SIZE sizeof(struct udp_struct)
#define CLIENT_SIZE sizeof(client)
#define SOCKADDR_SIZE sizeof(struct sockaddr)
#define SOCKADDRIN_SIZE sizeof(struct sockaddr_in)
#define MAX_TOPICS 100000

// macro pentru tratarea erorilor
#define DIE(assertion, call_description)  \
	do                                    \
	{                                     \
		if (assertion)                    \
		{                                 \
			fprintf(stderr, "(%s, %d): ", \
					__FILE__, __LINE__);  \
			perror(call_description);     \
			exit(EXIT_FAILURE);           \
		}                                 \
	} while (0)

// structura pentru un pachet
typedef struct packet
{
	char type;
	char topic[101];
} packet;

// structura pentru pachetele UDP
typedef struct udp_struct
{

	char topic[50];
	uint8_t type;
	char buff[1501];
} udp_struct;

// structura pentru pachetele TCP
typedef struct tcp_struct
{

	char ip[16];
	uint16_t port;
	char topic[51];
	char type[11];
	char buff[1501];
} tcp_struct;

// structura pentru un topic
typedef struct topic
{
	char name[51];
} topic;

// structura pentru un client
typedef struct client
{
	int topics_len;
	topic topics[100];
	char id[10];
	int socket;
	bool online;
} client;

// functie pentru actualizarea numarului de socket-uri
int update_socket_nr(int a, int b)
{
	return a > b ? a : b;
}

// functie pentru initializarea adresei serverului
struct sockaddr_in init_server(int server_port)
{
	struct sockaddr_in server_address;
	server_address.sin_port = htons(server_port);
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_family = AF_INET;
	return server_address;
}

// functie pentru inchiderea tuturor socket-urilor
void close_everything(int socket_num, fd_set fd_out)
{
	int i;
	for (i = 1; i <= socket_num; i++)
	{
		int check = FD_ISSET(i, &fd_out);
		if (!check)
		{
			continue;
		}
		close(i);
	}
}

// functie pentru gasirea unui client in functie de id ul sau
int find_client(client clients[], int socket_num, char *id)
{
	int i = 3;
	while (i <= socket_num)
	{
		if (strcmp(clients[i].id, id) == 0)
		{

			return i;
		}
		i++;
	}
	return -1;
}

// functie pentru gasirea unui client in functie de socket
int find_client_idx(client clients[], int i, int n)
{
	int index = -1;
	int j;
	for (j = 1; j <= n; j++)
	{
		if (i == clients[j].socket)
		{
			index = i;
			break;
		}
	}
	return index;
}

// functie pentru abonarea unui client la un topic
void subscribe(client *c, char *topic)
{
	strcpy(c->topics[c->topics_len].name, topic);
	c->topics_len++;
}

// functie pentru a verifica daca un client este abonat la un topic
int find_topic_index(struct client *c, const char *topic_name)
{
	for (int j = 0; j < c->topics_len; j++)
	{
		if (strcmp(c->topics[j].name, topic_name) == 0)
		{
			return j;
		}
	}
	return -1;
}

// functie pentru adaugarea unui nou topic la care este abonat clientul
void add_topic(struct client *c, const char *topic_name)
{
	if (c->topics_len < 100000)
	{
		strcpy(c->topics[c->topics_len].name, topic_name);
		c->topics_len++;
	}
}

// functie pentru stergerea unui topic din lista clientului
void remove_topic(struct client *c, int index)
{
	DIE(index < 0 || index >= c->topics_len, "invalid index");

	// am utilizat memmove pentru a muta toate elementele care vin dupa index cu o pozitie inapoi
	memmove(&c->topics[index], &c->topics[index + 1], (c->topics_len - index - 1) * sizeof(struct topic));
	c->topics_len--;
}

// functie pentru iesirea din program
int try_to_exit(char *buffer)
{
	memset(buffer, 0, 100);
	fgets(buffer, 100, stdin);
	char *command = NULL;
	strncpy(command, buffer, 4);
	command[4] = '\0';
	if (strcmp(command, "exit") == 0)
	{
		return 1;
	}
	return -1;
}

// functie pentru resetarea unui client
void reset_client(client *client)
{
	client->socket = -1;
	client->online = false;
}

int main(int argc, char **argv)
{
	// verificare - validitate comanda
	DIE(argc < 2, "comanda nu a fost data corect!");

	// dezactivare bufferizare pentru stdout
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// verificare - validitatea portului
	int server_port = atoi(argv[1]);
	DIE(server_port == 0 && strcmp(argv[1], "0") != 0, "port invalid");

	// vector de clienti
	client clients[1000];
	int check, i, j, socket_num;

	// am initializat serverul
	struct sockaddr_in server_address = init_server(server_port);
	struct sockaddr_in udp_address = init_server(server_port);
	struct sockaddr_in tcp_connect;

	// creare socket TCP
	int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_socket < 0, "eroare socket");
	// am legat socketul creat de adresa severului
	bind(tcp_socket, (struct sockaddr *)&server_address, sizeof(struct sockaddr));
	// dezactivare nagle
	setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char *)1, 4);
	// asteptare conexiuni pe socket
	int tcp_listen = listen(tcp_socket, INT_MAX);
	DIE(tcp_listen < 0, "eroare de (listen)");

	// creare socket UDP
	int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_socket < 0, "eroare socket");
	// am legat socketul creat de adresa severului
	bind(udp_socket, (struct sockaddr *)&udp_address, sizeof(struct sockaddr));

	// initializare descriptori de fisiere pentru select
	fd_set fd_out, fd_aux;
	FD_SET(tcp_socket, &fd_out);
	FD_SET(udp_socket, &fd_out);
	FD_SET(STDIN_FILENO, &fd_out);

	// numar maxim de socketuri
	socket_num = update_socket_nr(udp_socket, tcp_socket);

	bool run = true;
	while (run)
	{

		char buffer[PACKET_SIZE];
		// am folosit o variabila auxiliara pentru select
		fd_aux = fd_out;
		int sel = select(socket_num + 1, &fd_aux, NULL, NULL, NULL);
		DIE(sel < 0, "select");

		//  parcurgerea socketurilor
		for (i = 0; i <= socket_num; i++)
		{
			// verific ca pot citi pe socketul curent
			check = FD_ISSET(i, &fd_aux);
			memset(buffer, 0, PACKET_SIZE);

			// verific daca am eveniment pe stdin
			if (i == STDIN_FILENO && check)
			{
				// functia de oprire
				int exit = try_to_exit(buffer);
				if (exit == 1)
				{
					run = false;
					break;
				}
			}
			// verific daca am eveniment pe tcp
			else if (i == tcp_socket && check)
			{
				socklen_t len = SOCKADDR_SIZE;
				// socket nou pentru a comunica
				int socket = accept(tcp_socket, (struct sockaddr *)&tcp_connect, &len);
				DIE(socket < 0, "eroare accept tcp");
				// dezactivare nagle
				setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *)1, 4);
				// mesaj id client
				DIE(recv(socket, buffer, 10, 0) < 0, "recv");
				bool online;
				char *name;
				char *ip = inet_ntoa(tcp_connect.sin_addr);
				uint16_t port = ntohs(tcp_connect.sin_port);
				// caut clientul cu id ul dat in vectorul de clienti
				int index = find_client(clients, socket_num, buffer);
				// daca l-am gasit ii pastrez id ul si retin daca este sau nu online
				if (index != -1)
				{
					online = clients[index].online;
					name = clients[index].id;
				}
				// client nou
				if (index == -1)
				{
					// actualizez nr de socketuri
					socket_num = update_socket_nr(socket_num, socket);

					// il adaug la finalul vectorului de clienti
					strcpy(clients[socket_num].id, buffer);
					clients[socket_num].online = true;
					clients[socket_num].socket = socket;
					name = clients[socket_num].id;

					printf("New client %s connected from %s:%d.\n", name, ip, port);
				}
				// client existent
				else if (index)
				{
					// daca este deja online
					if (online)
					{
						// inchid socketul si trec peste urmatoarele instructiuni
						close(socket);
						printf("Client %s already connected.\n", name);
						continue;
					}
					else
					{
						// actualizez informatiile despre client
						printf("New client %s connected from %s:%d.\n", name, ip, port);

						clients[index].online = true;
						clients[index].socket = socket;
					}
				}
				// adaug socketul nou la setul de socketuri de iesire
				FD_SET(socket, &fd_out);
			}
			// verific daca am eveniment pe udp
			else if (i == udp_socket && check)
			{
				socklen_t len_0 = SOCKADDR_SIZE;
				memset(buffer, 0, PACKET_SIZE);
				int err = recvfrom(udp_socket, buffer, UDP_SIZE, 0, (struct sockaddr *)&udp_address, &len_0);
				DIE(err < 0, "recv udp");
				// structuri pentru datele primite si pentru cele trimise
				udp_struct *udp;
				tcp_struct tcp;
				double real;

				// ce am primit
				udp = (udp_struct *)buffer;

				// am convertit portul in format retea
				tcp.port = htons(udp_address.sin_port);

				// am copiat adresa IP a clientului UDP în campul IP al structurii TCP
				inet_ntop(AF_INET, &(udp_address.sin_addr), tcp.ip, INET_ADDRSTRLEN);

				// am copiat topicul din pachetul UDP in TCP
				strcpy(tcp.topic, udp->topic);
				tcp.topic[50] = '\0'; // topicul nu poate avea mai mult de 50 de caractere
				int num;
				// daca am numar real sau intreg, trebuie sa verific daca e negativ sau nu si sa ma ocup de zecimale
				if (udp->type == 0)
				{
					// numar natural
					uint32_t nr = *(uint32_t *)(udp->buff + 1);
					num = ntohl(nr);
					if (udp->buff[0] == 1)
					{
						// numar negativ
						num = -num;
					}
				}
				if (udp->type == 2)
				{
					// numar real
					int p10 = 1;
					int ok = udp->buff[5];
					while (ok > 0)
					{
						p10 *= 10;
						ok--;
					}
					uint32_t nr = *(uint32_t *)(udp->buff + 1);
					real = (double)ntohl(nr) / p10;
					if (udp->buff[0] == 1)
					{
						// numar negativ
						real = -real;
					}
				}
				// am analizat tipul datelor primite pt a construi structura tcp
				switch (udp->type)
				{
				case 0:
					// intreg
					sprintf(tcp.buff, "%d", num);
					strcpy(tcp.type, "INT");
					break;
				case 1:
					// numar real short
					sprintf(tcp.buff, "%.2f", (double)abs(ntohs(*(uint16_t *)(udp->buff))) / 100);
					strcpy(tcp.type, "SHORT_REAL");
					break;
				case 2:
					// real
					sprintf(tcp.buff, "%lf", real);
					strcpy(tcp.type, "FLOAT");
					break;
				case 3:
					// sir de caractere
					strcpy(tcp.type, "STRING");
					strcpy(tcp.buff, udp->buff);
					break;
				default:
					strcpy(tcp.type, "UNKNOWN");
					break;
				}

				j = 1;
				// am parcurs toti clientii si am trimis datele la cei abonati
				while (j <= socket_num)
				{
					int num_of_topics = clients[j].topics_len;
					int k = 0;
					while (k < num_of_topics)
					{
						char *topic_name = clients[j].topics[k].name;
						if (strcmp(topic_name, tcp.topic) == 0)
						{
							// daca clientul e abonat la topic si e online il trimitem
							if (clients[j].online)
							{
								DIE(send(clients[j].socket, &tcp, TCP_SIZE, 0) < 0, "send");
							}
						}
						k++;
					}
					j++;
				}
			}
			// verific daca am ceva de la client
			else if (check && recv(i, buffer, PACKET_SIZE, 0))
			{
				client *c;
				// am identificat clientul asociat socketului curent
				int index = find_client_idx(clients, i, socket_num);
				c = &clients[index];

				// am extras pachetul primit
				packet *new_pack = (packet *)buffer;
				char command = new_pack->type;
				char *topic = new_pack->topic;
				int topicIndex, s;

				// mai multe cazuri in functie de tipul pachetului
				switch (command)
				{
				case 's':
					// subscribe
					topicIndex = find_topic_index(c, topic);
					// daca clientul nu era abonat la topic, l-am abonat
					if (topicIndex < 0)
					{
						add_topic(c, topic);
					}
					break;

				case 'u':
					// unsubscribe
					topicIndex = find_topic_index(c, topic);
					// daca clientul e abonat la topicu, l-am dezabonat
					if (topicIndex >= 0)
					{
						remove_topic(c, topicIndex);
					}
					break;

				case 'e':
					// exit
					s = find_client_idx(clients, i, socket_num);
					// am deconectat clientul asociat socketului
					if (s)
					{
						FD_CLR(i, &fd_out);
						printf("Client %s disconnected.\n", clients[s].id);
						reset_client(&clients[s]);
					}
					break;

				default:
					break;
				}
			}
		}
	}
	close_everything(socket_num, fd_out);
	return 0;
}