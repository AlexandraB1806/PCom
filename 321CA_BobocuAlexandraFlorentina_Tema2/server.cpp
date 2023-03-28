#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"

#include <iostream>
#include <vector>
#include <unordered_map>

using namespace std;

// Structura unui client TCP
typedef struct {
    string id;
    string ip;
    int port;
    int socket;
    int status;
    unordered_map<string, int> topics_and_sf;
} __attribute__ ((__packed__)) tcp_client;

// Map de la socket-ul unui client pana la clientul propriu-zis
unordered_map<int, tcp_client*> connected_clients;
// Map de la numele unui topic la vectorul de clienti propriu-zisi
// abonati la acel topic
unordered_map<string, vector<tcp_client*>> topics;
// Map de la ID-ul clientului catre clientul propriu-zis
unordered_map<string, tcp_client*> id_clients;
// Map de la ID-ul clientului la socket
unordered_map<string, int> socket_clients;

void usage(char *file) {
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 2) {
		usage(argv[0]);
	}

    int n, ret;
    char buffer[MAX_LENGHT_CONTENT];

    fd_set read_fds, tmp_fds;
    int fdmax;
    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

    int new_sock_tcp;
    int i;
    struct sockaddr_in server_addr, client_addr, udp_addr;

    int portno;
	portno = atoi(argv[1]);
	DIE(portno == 0, "Cannot atoi");

    memset((char *) &server_addr, 0, sizeof(server_addr));
    // Se completeaza campurile structurii cu informatiile corespunzatoare
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portno);
	server_addr.sin_addr.s_addr = INADDR_ANY;

    // Socket corespunzator protocolului TCP
	int sock_tcp;
	// Creare socket TCP si alocare resurse de sistem
	sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sock_tcp < 0, "Error: Cannot read TCP socket!");

    // Oprirea algoritmului Nagle
	int nagle = 0;
	n = setsockopt(sock_tcp, IPPROTO_TCP,
				TCP_NODELAY, (char *)&nagle, sizeof(int));
	DIE(n < 0, "Error: Cannot stop Nagle algorithm!");

    ret = bind(sock_tcp, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "Error: Cannot bind TCP!");

    ret = listen(sock_tcp, MAX_CLIENTS);
	DIE(ret < 0, "listen");

    // Socket corespunzator protocolului UDP
	int sock_udp;
	// Creare socket UDP si alocare resurse de sistem
	sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sock_udp < 0, "Error: Cannot read UDP socket!");

    ret = bind(sock_udp, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "Error: Cannot bind UDP!");

    FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sock_tcp, &read_fds);
	FD_SET(sock_udp, &read_fds);

	fdmax = max (sock_tcp, sock_udp);

    socklen_t client_len;

    while (1) {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

        // Parcurg toti socketii posibili
        for (i = 0; i <= fdmax; i++) {
            // Daca socket-ul curent se gaseste in multimea temporara
            // de socketi
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == STDIN_FILENO) {
                    // Citesc un string gol
                    string empty;
					cin >> empty;
                    // Daca se primeste comanda "exit", se inchid simultan
                    // serverul si toti clientii conectati pana in acel moment
					if (empty.compare("exit") == 0) {
                        // Parcurg toti clientii conectati si ii anunt
                        // ca serverul se va inchide
                        for (auto cli : connected_clients) {
                            // Extrag socketul clientului conectat
                            auto socket_client = cli.first;

                            // Mesajul ce va fi trimis clientului
                            tcp_client_message message;
                            memset(&message, 0, sizeof(message));
                            memcpy(message.topic, "EXIT", strlen("EXIT") + 1);

                            // Trimit pe socketul clientului conectat mesajul
                            // cu topicul "EXIT"
                            ret = send(socket_client, &message, sizeof(message), 0);
                            DIE(ret < 0, "send");
                        }
                        // Inchidere conexiuni
						close(sock_tcp);
						close(sock_udp);

						return 0;
					}
                } else if (i == sock_tcp) {
                    // Un client nou TCP vrea sa se conecteze la server
                    client_len = sizeof(client_addr);

                    // Cererea a venit de pe socketul inactiv, pe care serverul
                    // o va accepta
                    new_sock_tcp = accept(sock_tcp,
                                    (struct sockaddr *) &client_addr,
                                    &client_len);
                    DIE (new_sock_tcp < 0, "Error: Cannot accept new TCP clients!");

                    memset(buffer, 0, MAX_LENGHT_CONTENT);
                    // Bufferul se incarca cu ID-ul clientului obtinut din
                    // noul socket
                    n = recv(new_sock_tcp, buffer, MAX_LENGHT_CONTENT, 0);
                    DIE (n < 0, "recv");

                    string aux = string(buffer);

                    // Verific daca exista deja un client conectat cu ID-ul
                    // respectiv. In caz afirmativ, ultimul client care a
                    // incercat sa se conecteze cu un ID deja folosit se
                    // va deconecta.
                    if (id_clients.find(aux) != id_clients.end()) {
                        if (id_clients[aux]->status == 1) {
                            cout << "Client " << aux
                                << " already connected.\n";

                            tcp_client_message message;
                            memset(&message, 0, sizeof(message));
                            memcpy(message.topic, "EXIT", strlen("EXIT") + 1);

                            ret = send (new_sock_tcp, &message,
                                        sizeof(tcp_client_message), 0);
                            DIE (ret < 0, "send");

                            continue;
                        }
                    }

                    // Adaug noul socket in multimea de file descriptori
                    FD_SET(new_sock_tcp, &read_fds);
                    // Actualizez socket-ul maxim
                    fdmax = max (fdmax, new_sock_tcp);

                    // Populez noul string cu ID-ul clientului existent
                    // in buffer
                    string client_id(buffer);

                    // Daca s-a gasit ID-ul clientului in map-ul id_clients,
                    // inseamna ca respectivul client a fost creat in prealabil
                    if (id_clients.find(client_id) != id_clients.end()) {
                        // Completez map-ul cu clientii care s-au conectat
                        connected_clients[new_sock_tcp] = id_clients[client_id];
                        // Setez statusul 1 (client conectat)
                        connected_clients[new_sock_tcp]->status = 1;
                        id_clients[client_id]->status = 1;
                        // Includ si noul socket in map-ul corespunzator
                        socket_clients[client_id] = new_sock_tcp;
                    } else {
                        // Daca nu s-a gasit ID-ul clientului in map, atunci
                        // trebuie sa cream un nou client care doreste sa se
                        // conecteze in map-ul corespunzator.
                        // Creez o instanta de client TCP si ii completez campurile
                        tcp_client *new_tcp_client = new tcp_client;
                        new_tcp_client->id = client_id;
                        new_tcp_client->ip = inet_ntoa(client_addr.sin_addr);
                        new_tcp_client->port = client_addr.sin_port;
                        new_tcp_client->socket = new_sock_tcp;
                        new_tcp_client->status = 1;

                        // Actualizez map-urile cu noul client adaugat
                        connected_clients[new_sock_tcp] = new_tcp_client;
                        id_clients[client_id] = new_tcp_client;

                        // Includ si noul socket in map-ul corespunzator
                        socket_clients[client_id] = new_sock_tcp;
                    }

					cout << "New client " << connected_clients[new_sock_tcp]->id
                         << " connected from "
						 << inet_ntoa(client_addr.sin_addr)
                         << ":" << htons(client_addr.sin_port) << ".\n";

                } else if (i == sock_udp) {
                    socklen_t len = sizeof(udp_addr);

                    // Mesajul UDP
					udp_client_message *msg = new udp_client_message();
                    memset(msg, 0, sizeof(udp_client_message));
					n = recvfrom(sock_udp, msg, sizeof(udp_client_message), 0, (struct sockaddr*)&udp_addr, &len);
					DIE(n < 0, "Error: Cannot receive UDP client message!");

                    // Mesajul TCP
					tcp_client_message *tcp_message = new tcp_client_message();
                    inet_ntop(AF_INET, &(udp_addr.sin_addr), tcp_message->ip,
                              INET_ADDRSTRLEN);
					tcp_message->port = htons(udp_addr.sin_port);
					memcpy(tcp_message->topic, msg->topic, sizeof(msg->topic));
                    tcp_message->data_type = msg->data_type;
					memcpy(tcp_message->content, msg->content,
                            sizeof(msg->content));

                    string aux = string(tcp_message->topic);
                    // S-a gasit topicul in map
                    if (topics.find(aux) != topics.end()) {
                        // Salvez intrarea din map
                        auto topic_entry = topics.find(tcp_message->topic);
                        // Parcurg toti clientii abonati la acel topic
                        for (auto subscriber : topic_entry->second) {
                            // Client conectat
                            if (id_clients[subscriber->id]->status == 1) {
                                // Salvez socketul si trimit pe acesta mesajul
                                // clientului care este conectat
                                int socket = socket_clients[subscriber->id];
                                ret = send(socket, tcp_message, sizeof(tcp_client_message), 0);
                                DIE (ret < 0, "send");
                            }
                        }
                    }
                } else {
                    // Se primesc pachete de la un client deja conectat, deci
                    // direct de pe socket-ul clientului
                    memset(buffer, 0, MAX_LENGHT_CONTENT);
                    // Bufferul se incarca cu comanda de pe socket
                    n = recv(i, buffer, MAX_LENGHT_CONTENT, 0);
			        DIE (n < 0, "recv");

                    // La incheierea conexiunii voi parcurge mai multi pasi:
                    // - afisez mesajul corespunzator;
                    // - actualizez statusul;
                    // - inchid socket-ul;
                    // - sterg din multimea de socketi socketul clientului pe
                    // care tocmai l-am deconectat.
                    // In plus, actualizez corespunzator map-urile.
                    if (n == 0) {
                        cout << "Client " << connected_clients[i]->id << " disconnected." << "\n";
                        // Salvez ID-ul clientului care urmeaza sa se deconecteze
                        string id_aux = connected_clients[i]->id;
                        id_clients[id_aux]->status = 0;
                        close(i);
                        FD_CLR(i, &read_fds);
                        // Sterg socketul clientului deconectat
                        socket_clients.erase(connected_clients[i]->id);
                        // Sterg clientul deconectat din map
                        connected_clients.erase(i);

                        continue;
                    }

                    if (strncmp(buffer, "subscribe", LEN_SUBSCR) == 0) {
                        // Impart mesajul primit in 3 bucati:
                        // - extrag comanda de subscribe;
                        // - extrag topicul mesajului;
                        // - extrag indicatorul store & forward.
                        char *command = strtok (buffer, " ");
                        char *topic = strtok (NULL, " ");
                        char *sf = strtok (NULL, "\n");

                        // Completarea numelui topicului
                        string name_topic(topic);

                        // Am gasit cheia (topicul) in map pentru accesarea valorii,
                        // adica a vectorului de clienti TCP
                        if (topics.find(name_topic) != topics.end()) {
                            // Presupunem ca nu am gasit clientul
                            bool ok = 0;
                            // Iterez prin clientii abonati la topic
                            for (auto subscriber : topics[name_topic]) {
                                // Daca ID-ul clientului curent coincide cu
                                // ID-ul clientului conectat
                                if (subscriber->id == connected_clients[i]->id) {
                                    // Completez in structura SF-ul
                                    subscriber->topics_and_sf[name_topic] = atoi(sf);
                                    // Client gasit
                                    ok = 1;
                                    break;
                                }
                            }
                            if (ok == 0) {
                                // Creez un nou client si completez campurile
                                tcp_client *new_tcp_client = new tcp_client;
                                new_tcp_client->id = connected_clients[i]->id;
                                new_tcp_client->ip = inet_ntoa(client_addr.sin_addr);
                                new_tcp_client->port = client_addr.sin_port;
                                new_tcp_client->socket = i;
                                new_tcp_client->status = 1;
                                new_tcp_client->topics_and_sf[connected_clients[i]->id] = atoi(sf);
                                // Actualizez si map-ul topics
                                topics[name_topic].push_back(new_tcp_client);
                            }
                        } else {
                            tcp_client *new_tcp_client = new tcp_client;
                            new_tcp_client->id = connected_clients[i]->id;
                            new_tcp_client->ip = inet_ntoa(client_addr.sin_addr);
                            new_tcp_client->port = client_addr.sin_port;
                            new_tcp_client->socket = i;
                            new_tcp_client->status = 1;
                            new_tcp_client->topics_and_sf[connected_clients[i]->id] = atoi(sf);

                            vector<tcp_client*> clients;
                            clients.push_back(new_tcp_client);
                            topics[name_topic] = clients;
                        }
                    } else if (strncmp(buffer, "unsubscribe", LEN_UNSUBSCR) == 0) {
                        // Impart mesajul primit in 2 bucati:
                        // - extrag comanda de subscribe/ unsubscribe/ exit;
                        // - extrag topicul mesajului
                        char* command = strtok (buffer, " ");
                        char* topic = strtok (NULL, " ");

                        // Completarea numelui topicului
                        string name_topic(topic);

                        // Nu am gasit clientul conectat
                        if (connected_clients.find(i) == connected_clients.end()) {
                            continue;
                        }

                        // Am gasit cheia (topicul) in map
                        if (topics.find(name_topic) != topics.end()) {
                            for (int j = 0; j < topics[name_topic].size(); j++) {
								if (topics[name_topic][j]->socket == socket_clients[name_topic]) {
									topics[name_topic].erase(topics[name_topic].begin() + j);
								}
							}
                        }
                    }
                }
            }
        }
    }
}
