/*
 * main.c
 *
 * UDP Server - Meteo
 *
 * Portabile su Windows, Linux e macOS
 */

#if defined WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 46
#endif

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include "protocol.h"

#if defined WIN32
typedef int socklen_t;
#endif

// Controlla che il tipo richiesto sia valido
int valida_tipo(char t) {
    return (t == 't' || t == 'h' || t == 'w' || t == 'p');
}

// Confronta due città ignorando maiuscolo/minuscolo
int confronta_citta(const char *c1, const char *c2) {
    while (*c1 && *c2) {
        if (tolower((unsigned char)*c1) != tolower((unsigned char)*c2))
            return 0;
        c1++;
        c2++;
    }
    return (*c1 == '\0' && *c2 == '\0');
}

// Verifica se la città è supportata
int citta_supportata(const char *city) {
    const char *lista[] = {
        "bari","roma","milano","napoli","torino",
        "palermo","genova","bologna","firenze","venezia"
    };
    int n = sizeof(lista) / sizeof(lista[0]);

    for (int i = 0; i < n; i++) {
        if (confronta_citta(city, lista[i]))
            return 1;
    }
    return 0;
}

// ---------------------------
// Funzioni meteo
// ---------------------------

static void init_random() {
    static int initialized = 0;
    if (!initialized) {
        srand((unsigned int)time(NULL));
        initialized = 1;
    }
}

float get_temperature(void) {
    init_random();
    return -10.0f + (rand() % 5000) / 100.0f;
}

float get_humidity(void) {
    init_random();
    return 20.0f + (rand() % 8000) / 100.0f;
}

float get_wind(void) {
    init_random();
    return (rand() % 10000) / 100.0f;
}

float get_pressure(void) {
    init_random();
    return 950.0f + (rand() % 10000) / 100.0f;
}

// ---------------------------
// Funzioni di sistema
// ---------------------------

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void errorhandler(const char *errorMessage) {
    printf("%s", errorMessage);
}

// Validazione semplice del nome città (caratteri non ammessi => invalid request)
int is_valid_city_string(const char *s) {
    // accetta lettere (A-Z a-z), spazi, apostrofo, trattino
    // rifiuta tabulazioni, caratteri di controllo e simboli come @ # $ % ecc.
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        unsigned char c = *p;
        if (c == '\t' || c == '\r' || c == '\n')
        	return 0;
        if ( (c >= 'A' && c <= 'Z') ||
             (c >= 'a' && c <= 'z') ||
             (c == ' ') || (c == '\'') || (c == '-') )
            continue;
        // accetta anche lettere accentate ( > 127 ), ma verificazione semplice:
        if (c >= 0x80)
        	continue;
        // altrimenti rifiuta
        return 0;
    }
    return 1;
}

// Helper: convert float -> network-order uint32_t
uint32_t float_to_netuint32(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(float));
    bits = htonl(bits);
    return bits;
}

// Helper: convert network-order uint32_t -> float
float netuint32_to_float(uint32_t netbits) {
    uint32_t hostbits = ntohl(netbits);
    float f;
    memcpy(&f, &hostbits, sizeof(float));
    return f;
}

// ---------------------------
// MAIN SERVER
// ---------------------------

int main(int argc, char *argv[]) {

#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != 0) {
        printf("Error in WSAStartup()\n");
        return 0;
    }
#endif

    unsigned short server_port = SERVER_PORT;

    // Parsing parametri riga di comando (-p port)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            server_port = (unsigned short)atoi(argv[i + 1]);
            i++;
        }
    }

    int my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket < 0) {
        errorhandler("creazione della socket UDP fallita.\n");
        clearwinsock();
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(my_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        errorhandler("bind() fallito.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    printf("Server UDP in ascolto sulla porta %d...\n", server_port);

    // Buffer per ricevere richieste
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        // ricevi datagramma
        memset(buffer, 0, sizeof(buffer));
        int rcv_bytes = recvfrom(my_socket, buffer, sizeof(buffer), 0,
                                 (struct sockaddr*)&client_addr, &client_len);
        if (rcv_bytes < 0) {
            errorhandler("recvfrom() fallito.\n");
            continue;
        }

        // risoluzione reverse DNS client (hostname) -- potrebbe fallire
        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, sizeof(client_ip_str));
        char client_hostbuf[NI_MAXHOST] = {0};
        struct hostent *client_he = NULL;
        client_he = gethostbyaddr((char*)&client_addr.sin_addr.s_addr, sizeof(client_addr.sin_addr.s_addr), AF_INET);
        if (client_he && client_he->h_name) {
            strncpy(client_hostbuf, client_he->h_name, sizeof(client_hostbuf)-1);
            client_hostbuf[sizeof(client_hostbuf)-1] = '\0';
        } else {
            // fallback all'indirizzo IP come nome
            strncpy(client_hostbuf, client_ip_str, sizeof(client_hostbuf)-1);
            client_hostbuf[sizeof(client_hostbuf)-1] = '\0';
        }

        // Deserializzazione REQUEST (type + city)
        weather_request_t req;
        memset(&req, 0, sizeof(req));
        if (rcv_bytes < 1) {
            // pacchetto troppo corto -> richiesta invalida
            printf("Pacchetto ricevuto troppo corto da %s (ip %s)\n", client_hostbuf, client_ip_str);
            continue;
        }

        // primo byte = type
        req.type = buffer[0];

        // resto = city string (se presente). Copio fino a CITY_NAME_LEN-1 o fino ai rcv_bytes
        int copy_len = rcv_bytes - 1;
        if (copy_len > (int)CITY_NAME_LEN - 1) copy_len = (int)CITY_NAME_LEN - 1;
        if (copy_len > 0) {
            memcpy(req.city, buffer + 1, copy_len);
            req.city[copy_len] = '\0';
        } else {
            req.city[0] = '\0';
        }

        // Log richiesta (come richiesto: nome host e ip)
        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
               client_hostbuf, client_ip_str, req.type, req.city);

        // Preparazione risposta
        weather_response_t res;
        res.status = STATUS_INVALID_REQ;
        res.type = req.type;
        res.value = 0.0f;

        // Validazione tipo
        if (!valida_tipo(req.type)) {
            res.status = STATUS_INVALID_REQ;
            res.type = '\0';
        }
        // Verifica city string: tab o caratteri non ammessi -> richiesta invalida
        else if (strlen(req.city) == 0) {
            // città vuota -> città non disponibile (decidi comportamento): trattiamo come city not found
            res.status = STATUS_CITY_NOT_FOUND;
            res.type = '\0';
        }
        else if (!is_valid_city_string(req.city)) {

            int contiene_speciali = 0;

            for (const unsigned char *p = (unsigned char*)req.city; *p; p++) {
                if (*p == '@' || *p == '#' || *p == '$' || *p == '%')
                    contiene_speciali = 1;
            }

            if (contiene_speciali)
                res.status = STATUS_INVALID_REQ;   // caratteri speciali → INVALID_REQ
            else
                res.status = STATUS_CITY_NOT_FOUND; // numeri → CITY_NOT_FOUND

            res.type = '\0';
        }
        // città troppo lunga? (il client dovrebbe aver già validato)
        else if (strlen(req.city) >= CITY_NAME_LEN) {
            res.status = STATUS_INVALID_REQ;
            res.type = '\0';
        }
        // città non supportata
        else if (!citta_supportata(req.city)) {
            res.status = STATUS_CITY_NOT_FOUND;
            res.type = '\0';
        }
        else {
            // Valida: genera il valore meteo
            res.status = STATUS_SUCCESS;
            switch (req.type) {
                case 't': res.value = get_temperature();
                break;
                case 'h': res.value = get_humidity();
                break;
                case 'w': res.value = get_wind();
                break;
                case 'p': res.value = get_pressure();
                break;
                default: res.status = STATUS_INVALID_REQ; res.type = '\0';
                break;
            }
        }

        // Serializzazione della response in buffer separato:
        // layout: [uint32_t net_status][char type][uint32_t net_float_bits]
        unsigned char outbuf[sizeof(uint32_t) + sizeof(char) + sizeof(uint32_t)];
        int offset = 0;

        uint32_t net_status = htonl((uint32_t)res.status);
        memcpy(outbuf + offset, &net_status, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        char out_type = (res.status == STATUS_SUCCESS) ? res.type : '\0';
        memcpy(outbuf + offset, &out_type, sizeof(char));
        offset += sizeof(char);

        uint32_t net_float = float_to_netuint32(res.value);
        memcpy(outbuf + offset, &net_float, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Invio risposta al client (sendto)
        int sent = sendto(my_socket, (const char*)outbuf, offset, 0,
                          (struct sockaddr*)&client_addr, client_len);
        if (sent != offset) {
            errorhandler("sendto() ha inviato un numero di byte differente da quanto previsto\n");
            // continua comunque
        }
        // loop continua per prossime richieste
    }


    closesocket(my_socket);
    clearwinsock();
    return 0;
}
