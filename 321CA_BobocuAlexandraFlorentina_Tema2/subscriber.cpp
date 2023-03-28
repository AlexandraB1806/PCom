#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

#include <netinet/tcp.h>
#include <iomanip>
#include <math.h>
#include <iostream>

using namespace std;

void usage(char *file) {
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

// Functie in care prelucrez pachetele primite de pe socket-ul TCP in functie
// de tipul de date
void process_packets_socket(tcp_client_message *msg) {
	if ((int)msg->data_type == INT) {
		// Creez un mesaj de tip INT si il completez
		int_message *int_msg = new int_message();
		memcpy(int_msg, msg->content, sizeof(int_message));

		// Numarul pe 32 de biti formatat conform network byte order
		int num = ntohl(int_msg->number);
		
		// Setez octetul de semn
		if (int_msg->sign == 1) {
			num = num * -1;
		}

		cout << msg->ip << ':' << msg->port << " - " << msg->topic
			 << " - " << "INT" << " - " << num << '\n';
	} else if ((int)msg->data_type == SHORT_REAL) {
		// Creez un mesaj de tip SHORT_REAL si il completez
		short_real_message *short_real_msg = new short_real_message();
		memcpy(short_real_msg, msg->content, sizeof(uint16_t));

		// Numarul pe 16 de biti formatat conform network byte order
		short_real_msg->number = ntohs(short_real_msg->number);

		cout << msg->ip << ':' << msg->port << " - " << msg->topic
			 << " - " << "SHORT_REAL" << " - "
			 << fixed << setprecision(2)
			 << 1.0 * short_real_msg->number / 100 << "\n";
	} else if ((int)msg->data_type == FLOAT) {
		// Creez un mesaj de tip FLOAT si il completez
		float_message *float_msg = new float_message();
		memcpy(float_msg, msg->content, sizeof(float_message));

		// Numarul pe 32 de biti formatat conform network byte order
		float_msg->number = ntohl(float_msg->number);

		// Setez octetul de semn
		if (float_msg->sign == 1) {
			float_msg->number = float_msg->number * -1;
		}

		cout << msg->ip << ':' << msg->port << " - " << msg->topic
			 << " - " << "FLOAT" << " - "
			 << fixed << setprecision((int)float_msg->power)
			 << 1.0 * float_msg->number / pow(10, (int)float_msg->power)
			 << '\n';
	} else if ((int)msg->data_type == STRING) {
		cout << msg->ip << ':' << msg->port << " - " << msg->topic
			 << " - " << "STRING" << " - " << msg->content << '\n';
	}
}

int main (int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc < 3) {
		usage(argv[0]);
	}

	int n, ret;
	char buffer[MAX_LENGHT_CONTENT];

	// Socket corespunzator protocolului TCP
	int sock_tcp;
	// Creare socket TCP
	sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sock_tcp < 0, "Error: Cannot read TCP socket!");

	fd_set read_fds, tmp_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Oprirea algoritmului Nagle
	int nagle = 0;
	n = setsockopt(sock_tcp, IPPROTO_TCP,
				TCP_NODELAY, (char *)&nagle, sizeof(int));
	DIE(n < 0, "Error: Cannot stop Nagle algorithm!");

	// Adresa serverului
	struct sockaddr_in serv_addr;
	// Se completeaza campurile structurii cu informatiile corespunzatoare
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// Dupa crearea socketului si completarea adresei serverului,
	// facem conexiunea clientului catre server
	ret = connect(sock_tcp, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "Error: TCP Socket cannot connect!");

	// Completez bufferul cu ID-ul clientului
	memset(buffer, 0, MAX_LENGHT_CONTENT);
	memcpy(buffer, argv[1], strlen(argv[1]) + 1);

	// Trimitem ID-ul clientului pe socket
	send(sock_tcp, buffer, MAX_LENGHT_CONTENT, 0);

	// Adaug in multimea de file descriptori pe cel specific citirii de la
	// tastatura si pe cel citirii de pe socket
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sock_tcp, &read_fds);

	while (1) {
		tmp_fds = read_fds;

		ret = select(sock_tcp + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		// Daca se primesc comenzi de la tastatura,
		// trimit mai departe mesajul primit serverului
		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            memset(buffer, 0, MAX_LENGHT_CONTENT);
			// Citesc comanda
            fgets(buffer, MAX_LENGHT_CONTENT - 1, stdin);

			// Caz 1: Comanda de exit: deconectez clientul TCP de la
			// server si il inchid
			if (strncmp(buffer, "exit", LEN_EXIT) == 0) {
                break;
            }
			// Caz 2: Comanda de subscribe: trimit mesaj serverului
			// pentru ca s-a abonat clientul TCP la topic
            if (strncmp(buffer, "subscribe", LEN_SUBSCR) == 0) {
                n = send(sock_tcp, buffer, MAX_LENGHT_CONTENT, 0);
                DIE (n < 0, "send");
			
                cout << "Subscribed to topic.\n";
			
				continue;
            }
			// Caz 3: Comanda de unsubscribe: trimit mesaj serverului
			// pentru ca s-a dezabonat clientul TCP de la topic
            if (strncmp(buffer, "unsubscribe", LEN_UNSUBSCR) == 0) {
                n = send(sock_tcp, buffer, MAX_LENGHT_CONTENT, 0);
                DIE (n < 0, "send");
			
                cout << "Unsubscribed from topic.\n";
			
				continue;
            }
        }
		// Daca se primesc comenzi de pe socketul TCP
		if (FD_ISSET(sock_tcp, &tmp_fds)) {
			tcp_client_message *msg = new tcp_client_message;

			n = recv(sock_tcp, msg, sizeof(tcp_client_message), 0);
			DIE (n < 0, "recv");

			// Conexiune inchisa
			if (n == 0) {
				break;
			}

			if (strncmp(msg->topic, "EXIT", 4) == 0) {
				break;
			}

			process_packets_socket(msg);
		}
	}

	// Inchidere socket TCP
	close(sock_tcp);

	return 0;
}
