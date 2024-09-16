#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/select.h>

#define SOCKADDRIN_SIZE sizeof(struct sockaddr_in)
#define PACKET_SIZE sizeof(struct packet)
#define TCP_SIZE sizeof(struct tcp_struct)

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



// structura pentru pachetele TCP
typedef struct tcp_struct
{
	char ip[16];
	uint16_t port;
	char topic[51];
	char type[11];
	char buff[1501];
} tcp_struct;

// structura pentru topic
typedef struct topic
{
	char name[51];
} topic;

// structura pentru pachete de trimis
typedef struct packet
{
	char type;
	char topic[101];
} packet;

// functie pentru initializarea adresei serverului
struct sockaddr_in init_server(int server_port)
{
	struct sockaddr_in server_address;
	server_address.sin_port = htons(server_port);
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_family = AF_INET;
	return server_address;
}

int main(int argc, char **argv)
{
	// verificare - corectitudinea comenzii
	DIE(argc < 4, "comanda nu a fost data corect!");

	// dezactivare bufferizare pentru stdout
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	packet pack;
	int check, check1;
	char buffer[PACKET_SIZE];
	int port = atoi(argv[3]);

	// verificare - validitatea portului
	DIE(port == 0 && strcmp(argv[1], "0") != 0, "port invalid");

	// creare socket TCP
	int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

	// setarea optiunilor socketului pentru dezactivarea Nagle
	setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char *)1, 4);

	// verificare - eroare socket
	DIE(tcp_socket < 0, "eroare socket");

	// initializarea adresei serverului
	struct sockaddr_in server_address = init_server(port);
	inet_aton(argv[2], &server_address.sin_addr);

	// initializare descriptori de fisiere pentru select
	fd_set fd_out, fd_aux;
	FD_SET(tcp_socket, &fd_out);
	FD_SET(STDIN_FILENO, &fd_out);

	// conectare la server
	DIE(connect(tcp_socket, (struct sockaddr *)&server_address, SOCKADDRIN_SIZE) < 0, "eroare connect server");
	DIE(send(tcp_socket, argv[1], 10, 0) < 0, "send");

	// gestionare inputuri de la server sau de la stdin
	while (1)
	{
		fd_aux = fd_out;
		int ok = 0;
		DIE(select(tcp_socket + 1, &fd_aux, NULL, NULL, NULL) < 0, "select");
		check = FD_ISSET(STDIN_FILENO, &fd_aux);
		check1 = FD_ISSET(tcp_socket, &fd_aux);

		if (check)
		{
			// informatii primite de la stdin
			memset(buffer, 0, 100);
			fgets(buffer, 100, stdin);
			char type;
			// verific tipul comenzii primite in buffer
			int subscribe = strncmp(buffer, "subscribe", 9);
			int unsubscribe = strncmp(buffer, "unsubscribe", 11);
			int exit_client = strncmp(buffer, "exit", 4);
			// in functie de asta stabilesc tipul de pachet
			if (subscribe == 0)
			{
				type = 's';
			}
			if (unsubscribe == 0)
			{
				type = 'u';
			}
			if (exit_client == 0)
			{
				type = 'e';
			}

			switch (type)
			{
			case 's':
			{ // subscribe

				// am gasit pozitia spatiului in buffer
				char *space_position = strchr(buffer, ' ');

				// am verificat daca s-a gasit spatiul
				if (space_position != NULL)
				{
					// avansarea pozitiei pentru a ignora spatiul si a copia doar cuvantul urmator
					space_position++;
					// calculul lungimii cuvantului
					size_t word_length = strcspn(space_position, " \n");
					// copierea cuvantului in topicul pachetului
					memcpy(pack.topic, space_position, word_length);
					// m-am asigurat ca plasez terminatorul de sir
					pack.topic[word_length] = '\0';
				}
				pack.type = 's';
				DIE(send(tcp_socket, &pack, PACKET_SIZE, 0) < 0, "subscribe");
				printf("Subscribed to topic %s\n", pack.topic);
				break;
			}
			case 'u':
			{ // unsubscribe

				// similar cu subscribe
				char *space_position = strchr(buffer, ' ');
				if (space_position != NULL)
				{
					space_position++;
					size_t word_length = strcspn(space_position, " \n");
					memcpy(pack.topic, space_position, word_length);
					pack.topic[word_length] = '\0';
				}
				pack.type = 'u';
				DIE(send(tcp_socket, &pack, PACKET_SIZE, 0) < 0, "unsubscribe");
				printf("Unsubscribed from topic %s\n", pack.topic);
				break;
			}
			case 'e':
			{ // deconectare
				ok = 1;
				pack.type = 'e';
				DIE(send(tcp_socket, &pack, PACKET_SIZE, 0) < 0, "exit");
				break;
			}
			default:
			{
				printf("Input invalid.\n");
			}
			}
			// am intrerupt bucla in cazul deconectarii
			if (ok == 1)
			{
				break;
			}
		}
		if (check1)
		{
			// informatii primite de la server
			if (recv(tcp_socket, buffer, TCP_SIZE, 0))
			{
				tcp_struct *packet_to_send = (tcp_struct *)buffer;
				printf("%s:%u - %s - %s - %s\n", packet_to_send->ip, packet_to_send->port, packet_to_send->topic, packet_to_send->type, packet_to_send->buff);
			}
			else
			{
				break;
			}
		}
	}

	// inchidere socket TCP
	close(tcp_socket);
	return 0;
}
