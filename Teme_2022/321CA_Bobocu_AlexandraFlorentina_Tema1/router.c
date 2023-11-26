// Bobocu Alexandra-Florentina, 321CA
#include "queue.h"
#include "skel.h"

#define MAX_ENTRIES_RTABLE 100000
#define MAX_ENTRIES_ARP 1000

// Tabela de rutare
struct route_table_entry *rtable;

// Dimensiunea tabelei de rutare
int rtable_size;

// Tabela ARP
struct arp_entry *arp_table;

// Dimensiunea tabelei ARP
int arp_table_size;

// Functie de comparare pentru sortarea intrarilor in tabela
// de rutare dupa prefix. Comparare utila in cazul egalitatii mastilor.
int compare (const void *a, const void *b) {
    const struct route_table_entry *a_route = (const struct route_table_entry *)a;
	const struct route_table_entry *b_route = (const struct route_table_entry *)b;

	// Daca prefixele sunt diferite, sortez dupa prefix
	if ((a_route->prefix & a_route->mask) != (b_route->prefix & b_route->mask)) {
		return ntohl(b_route->prefix & b_route->mask) <
				ntohl(a_route->prefix & a_route->mask);
	} else {
		// Altfel, sortez dupa masca
		return ntohl(b_route->mask) < ntohl(a_route->mask);
	}
}

// Caut eficient intrarea in tabela de rutare folosind cautare binara. Functia
// returneaza -1 daca nu am gasit o intrare in tabela de rutare, altfel
// returneaza pozitia unde s-a gasit cea mai lunga masca, corespunzatoare
// intrarii gasite in tabela de rutare
int binary_search(int max_pos, uint32_t max_mask, int left, int right,
				uint32_t ip_destination) {
    if (right >= left) {
        int mid = left + (right - left) / 2;

		// Voi trece atat adresa IP destinatie, cat si prefixul prin masca,
		// efectuand operatia de & pe biti
        if ((ip_destination & rtable[mid].mask) ==
			(rtable[mid].prefix & rtable[mid].mask)) {
			// Determin cea mai lunga masca, dar si pozitia ei,
			// in momentul in care am gasit intrarea in tabela de rutare
			if (max_mask < ntohl(rtable[mid].mask)) {
				max_mask = ntohl(rtable[mid].mask);
				max_pos = mid;
			}
			return binary_search(max_pos, max_mask, mid + 1, right,
								ip_destination);
		}

		// Caut in prima jumatate
		if (ntohl(ip_destination & rtable[mid].mask) <
			ntohl((rtable[mid].prefix & rtable[mid].mask))) {
			return binary_search(max_pos, max_mask, left, mid - 1,
								ip_destination);
		}

		// Caut in a doua jumatate
        return binary_search(max_pos, max_mask, mid + 1, right,
							ip_destination);
    }
    return max_pos;
}

// Functie care imi returneaza intrarea din tabela ARP unde am gasit
// adresa IP a urmatorului hop
struct arp_entry *get_arp_entry(struct arp_entry *arp_table, int len,
								uint32_t ip) {
	for (int i = 0; i < len; i++) {
		if ((arp_table + i)->ip == ip) {
			return (arp_table + i);
		}
	}
	return NULL;
}

// Functie care calculeaza suma de control mai eficient,
// conform RFC 1624
void bonus_recalculate_checksum(struct iphdr *ip_hdr) {
	uint16_t old_checksum = ip_hdr->check;
	// Facem cast astfel incat ttl-ul sa fie retinut pe 16 biti
	uint16_t old_ttl = (ip_hdr->ttl & 0xffff);
	// Actualizez ttl-ul
	ip_hdr->ttl--;
	// Retin noul ttl
	uint16_t new_ttl = (ip_hdr->ttl & 0xffff);
	// Calculez checksum-ul folosind formula: HC' = ~(~HC + ~m + m')
	uint16_t new_checksum = ~(~old_checksum + ~old_ttl + new_ttl) - 1;
	// Actualizez campul corespunzator header-ului ip
	ip_hdr->check = new_checksum;
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);

	packet m;
	int rc;

	// Alocarea si parsarea tabelei de rutare
	rtable = calloc(sizeof(*rtable), MAX_ENTRIES_RTABLE);
	DIE(NULL == rtable, "Calloc for route table failed!");
	rtable_size = read_rtable(argv[1], rtable);

	// Alocarea tabelei ARP
	arp_table = calloc(sizeof(*arp_table), MAX_ENTRIES_ARP);
	DIE(NULL == arp_table, "Calloc for arp table failed!");

	// Sortarea corespunzatoare a tabelei de rutare
	qsort(rtable, rtable_size, sizeof(struct route_table_entry), compare);

	// Crearea cozii in care tin minte pachete
	queue package_queue = queue_create();

	init(argc - 2, argv + 2);

	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_packet");

		// Extrag header-ul Ethernet al pachetului primit
		struct ether_header *eth_hdr = (struct ether_header *)m.payload;

		// Daca am primit un pachet IP
		if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP) {
			struct iphdr *ip_hdr = (struct iphdr *)(m.payload + 
									sizeof(struct ether_header));

			// Returnez header-ul ICMP din bufferul m.payload
			struct icmphdr *icmp_hdr = (struct icmphdr *)(m.payload +
										sizeof(struct iphdr) +
										sizeof(struct ether_header));

			// Daca am primit un mesaj ICMP
			if (icmp_hdr != NULL) {
				// Acest mesaj este de tipul "Echo request"
				if (icmp_hdr->type == ICMP_ECHO) {
					// Acest mesaj este adresat ruterului,
					// deci este un pachet destinat lui insusi
					if (inet_addr(get_interface_ip(m.interface)) ==
						ip_hdr->daddr) {
						// Ruterul raspunde cu un mesaj de tip "Echo reply".
						// Completez interfata adresei fizice
						uint8_t mac[ETH_ALEN];
						get_interface_mac(m.interface, mac);
						// Trimit mesajul ICMP, interschimband sursa
						// cu destinatia si invers
						send_icmp(ip_hdr->saddr, ip_hdr->daddr, mac,
								eth_hdr->ether_shost, ICMP_ECHOREPLY,
								ICMP_ECHOREPLY, m.interface,
								icmp_hdr->un.echo.id, icmp_hdr->un.echo.sequence);
						continue;
					}
				}
			}

			// Verific TTL. Daca este mai mic sau egal decat 1,
			// pachetul este aruncat
			if (ip_hdr->ttl <= 1) {
				// Completez interfata adresei fizice
				uint8_t mac[ETH_ALEN];
				get_interface_mac(m.interface, mac);
				// Trimit mesajul ICMP de eroare specific expirarii TTL-ului.
				// Interschimb sursa cu destinatia si invers
				send_icmp_error(ip_hdr->saddr, ip_hdr->daddr, mac,
								eth_hdr->ether_shost, ICMP_TIME_EXCEEDED,
								ICMP_EXC_TTL, m.interface);
				continue;
			}

			// Verific checksum-ul. Daca suma de control a pachetului nu
			// coincide cu cea primita in antetul de IP, pachetul a fost
			// corupt, deci il arunc
			if (ip_checksum(ip_hdr, sizeof(struct iphdr)) != 0) {
				continue;
			}

			// Actualizez TTL-ul si recalculez checksum-ul eficient,
			// folosind algoritmul RFC 1624
			bonus_recalculate_checksum(ip_hdr);

			// Caut eficient in tabela de rutare adresa IP a urmatorului hop
			// folosind cautare binara
			struct route_table_entry best_route;
			int index = binary_search(-1, 0, 0, rtable_size - 1, ip_hdr->daddr);
			// Daca IP-ul urmatorului hop nu a fost gasit, arunc pachetul
			if (index == -1) {
				// Completez interfata adresei fizice
				uint8_t mac[ETH_ALEN];
				get_interface_mac(m.interface, mac);
				// Trimit mesajul ICMP de eroare specific negasirii
				// urmatorului hop. Interschimb sursa cu destinatia si invers
				send_icmp_error(ip_hdr->saddr,
							inet_addr(get_interface_ip(m.interface)),
							mac, eth_hdr->ether_shost, ICMP_DEST_UNREACH,
							ICMP_ECHOREPLY, m.interface);
				continue;
			} else {
				// Salvez ruta gasita
				best_route = rtable[index];
			}

			// Actualizez sursa adresei Ethernet
			get_interface_mac(best_route.interface, eth_hdr->ether_shost);

			// Avand adresa IP a urmatorului hop, gasesc intrarea in tabela ARP
			// unde am gasit aceasta adresa IP
			struct arp_entry *match_arp_entry = get_arp_entry(arp_table,
															arp_table_size,
															ip_hdr->daddr);
			// Nu am gasit adresa IP a urmatorului hop in tabela ARP
			if (match_arp_entry == NULL) {
				// Creez o copie a pachetului si o adaug in coada de pachete
				packet *copy_m = malloc(sizeof(*copy_m));
				memcpy(copy_m, &m, sizeof(packet));
				queue_enq(package_queue, copy_m);

				// Ruterul genereaza un mesaj de broadcast in retea pentru
				// aflarea adresei, deci setez destinatia adresei Ethernet cu 0xff
				memset(eth_hdr->ether_dhost, 0xff, ETH_ALEN);
				// Setez tipul adresei Ethernet
				eth_hdr->ether_type = htons(ETHERTYPE_ARP);

				// Trimit un mesaj de tip ARP Request catre urmatorul hop,
				// pentru a afla adresa MAC asociata adresei IP
				send_arp(best_route.next_hop,
						inet_addr(get_interface_ip(best_route.interface)),
						eth_hdr, best_route.interface, htons(ARPOP_REQUEST));
				continue;
			} else {
				// Am aflat adresa MAC asociata adresei IP, deci
				// completez campul corespunzator din header-ul Ethernet
				memcpy(eth_hdr->ether_dhost, match_arp_entry->mac, ETH_ALEN);
			}

			// Setez interfata pachetului
			m.interface = best_route.interface;
			
			// Trimit pachetul pe ruta gasita
			send_packet(&m);
		} else {
			// Am primit pachete de tip ARP
			struct arp_header *arp_hdr = (struct arp_header *)(m.payload +
										sizeof(struct ether_header));

			if (arp_hdr != NULL) {
				// Verific ce tip de pachet ARP am primit: Request sau Reply
				if (ntohs(arp_hdr->op) == ARPOP_REQUEST) {
					// Completez interfata adresei fizice
					uint8_t mac[ETH_ALEN];
					get_interface_mac(m.interface, mac);

					// Actualizez campurile Ethernet
					memcpy(eth_hdr->ether_shost, mac, ETH_ALEN);
					memcpy(eth_hdr->ether_dhost, arp_hdr->sha, ETH_ALEN);

					// Interschimb sursa cu destinatia si invers
					send_arp(arp_hdr->spa,
							inet_addr(get_interface_ip(m.interface)),
							eth_hdr, m.interface, htons(ARPOP_REPLY));

					continue;
				} else if (ntohs(arp_hdr->op) == ARPOP_REPLY) {
					// Adaug intrarea in tabela ARP 
					arp_table[arp_table_size].ip = arp_hdr->spa;
					memcpy(arp_table[arp_table_size].mac, arp_hdr->sha, ETH_ALEN);
					arp_table_size++;

					// Parcurg coada de pachete
					while (!queue_empty(package_queue)) {
						// Iau primul element din coada
						packet *packet_reply = (packet*)first_queue(package_queue);
						// Extrag headerele Ethernet si IP din acest pachet
						struct ether_header *packet_reply_eth =
								(struct ether_header*)packet_reply->payload;
						struct iphdr *packet_reply_ip = (struct iphdr*)
								(packet_reply->payload + sizeof(struct ether_header));

						// Gasesc ruta optima folosind cautare binara
						struct route_table_entry best_route;
						int index = binary_search(-1, 0, 0, rtable_size - 1,
									packet_reply_ip->daddr);
						// Daca IP-ul urmatorului hop nu a fost gasit, arunc pachetul
						if (index == -1) {
							continue;
						} else {
							// Salvez ruta gasita
							best_route = rtable[index];
						}

						// Verific daca primul pachet din coada are IP-ul in
						// tabela ARP
						if (best_route.next_hop != arp_hdr->spa) {
							// Astept alte arp replies
							break;
						} else {
							packet_reply = (packet *)queue_deq(package_queue);
						}

						memcpy(packet_reply_eth->ether_dhost,
								arp_hdr->sha, ETH_ALEN);
						get_interface_mac(best_route.interface,
								packet_reply_eth->ether_shost);
						packet_reply->interface = best_route.interface;

						// Trimit pachetul
						send_packet(packet_reply);
						// Odata ce am trimis pachetul, ii eliberez memoria
						free(packet_reply);
					}
					continue;
				}
			}
		}
	}
	// Eliberez memoria tabelelor
	free(rtable);
	free(arp_table);
}
