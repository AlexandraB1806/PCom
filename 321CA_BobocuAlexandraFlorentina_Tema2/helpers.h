#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define MAX_CLIENTS	1000
#define MAX_LENGHT_CONTENT 1500
#define TYPE_DATA 1
#define MAX_LENGTH_TOPIC 50
#define LEN_EXIT 4
#define LEN_SUBSCR 9
#define LEN_UNSUBSCR 11

enum Type {INT, SHORT_REAL, FLOAT, STRING};

typedef struct {
	char topic[MAX_LENGTH_TOPIC];
	char data_type;
	char content[MAX_LENGHT_CONTENT];
} __attribute__ ((__packed__)) udp_client_message;

typedef struct {
	char ip[16];
	int port;
	char topic[MAX_LENGTH_TOPIC];
	char data_type;
	char content[MAX_LENGHT_CONTENT];
} __attribute__ ((__packed__)) tcp_client_message;

typedef struct {
	// Octetul de semn:
	// 0 - numere pozitive
	// 1 - numere negative
	uint8_t sign;
	// Va fi formatat conform network -> byte order
	uint32_t number;
} __attribute__ ((__packed__)) int_message;

typedef struct {
	uint16_t number;
} __attribute__ ((__packed__)) short_real_message;

typedef struct {
	// Octetul de semn:
	// 0 - numere pozitive
	// 1 - numere negative
	uint8_t sign;
	// Va fi formatat conform network -> byte order
	int32_t number;
	uint8_t power;
} __attribute__ ((__packed__)) float_message;

#endif
