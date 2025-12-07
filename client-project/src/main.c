/*
 * main.c
 *
 * UDP Client - Meteo
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
#include <stdint.h>
#include "protocol.h"


void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void errorhandler(const char *msg) {
    printf("%s\n", msg);
}

void print_usage(const char *prog) {
    printf("Uso: %s [-s server] [-p port] -r \"type city\"\n", prog);
}

/* -------------------------
   Funzioni di conversione float <-> network order
-------------------------- */
uint32_t float_to_net(float f) {
    uint32_t t;
    memcpy(&t, &f, sizeof(float));
    return htonl(t);
}

float net_to_float(uint32_t n) {
    uint32_t t = ntohl(n);
    float f;
    memcpy(&f, &t, sizeof(float));
    return f;
}

int main(int argc, char *argv[]) {

    char *server_name = "localhost";
    int server_port = SERVER_PORT;
    char request_str[128] = {0};

    // Parsing argomenti

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            server_name = argv[++i];
        }
        else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            server_port = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) {
            strncpy(request_str, argv[++i], sizeof(request_str)-1);
        }
    }

    if (request_str[0] == '\0') {
        print_usage(argv[0]);
        return -1;
    }

#if defined WIN32
    WSADATA data;
    int result = WSAStartup(MAKEWORD(2,2), &data);
    if (result != 0) {
        printf("Errore in WSAStartup()\n");
        return 0;
    }
#endif

    /* -------------------------
       Risoluzione DNS
    -------------------------- */
    struct hostent *host = gethostbyname(server_name);
    if (!host) {
        errorhandler("Errore nella risoluzione DNS.");
        clearwinsock();
        return -1;
    }

    char server_ip_str[INET_ADDRSTRLEN];
    strcpy(server_ip_str, inet_ntoa(*(struct in_addr*)host->h_addr));

    /* -------------------------
       Parsing della richiesta -r "type city"
    -------------------------- */

    // Separazione type + city
    char *space = strchr(request_str, ' ');
    if (!space || space == request_str || strlen(space+1) == 0) {
        errorhandler("Richiesta non valida: formato -r \"type city\"\n");
        return -1;
    }

    char type = request_str[0];
    char *city = space + 1;

    if (strlen(city) >= CITY_NAME_LEN) {
        errorhandler("Errore: nome città troppo lungo.\n");
        return -1;
    }

    /* -------------------------
       Creazione socket UDP
    -------------------------- */
    int my_socket;
    if ((my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        errorhandler("Errore creazione socket UDP");
        clearwinsock();
        return -1;
    }

    /* -------------------------
       Configurazione indirizzo server
    -------------------------- */
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr = *((struct in_addr*)host->h_addr);

    /* -------------------------
       Serializzazione richiesta
    -------------------------- */
    char buffer[1 + CITY_NAME_LEN];
    int offset = 0;

    // type (1 byte)
    memcpy(buffer + offset, &type, 1);
    offset += 1;

    // city (stringa null-terminated)
    memcpy(buffer + offset, city, strlen(city)+1);
    offset += strlen(city)+1;

    /* -------------------------
       Invio richiesta
    -------------------------- */
    if (sendto(my_socket, buffer, offset, 0,
               (struct sockaddr*)&server_addr, server_len) != offset)
    {
        errorhandler("Errore sendto()");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    /* -------------------------
       Ricezione risposta
    -------------------------- */
    char resbuf[sizeof(uint32_t) + 1 + sizeof(uint32_t)];
    int res_size = recvfrom(my_socket, resbuf, sizeof(resbuf), 0,
                            (struct sockaddr*)&server_addr, &server_len);
    if (res_size <= 0) {
        errorhandler("Errore recvfrom()");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    /* -------------------------
       Deserializzazione risposta
    -------------------------- */
    int off = 0;

    uint32_t net_status;
    memcpy(&net_status, resbuf + off, sizeof(uint32_t));
    off += sizeof(uint32_t);
    unsigned int status = ntohl(net_status);

    char rtype;
    memcpy(&rtype, resbuf + off, 1);
    off += 1;

    uint32_t net_value;
    memcpy(&net_value, resbuf + off, sizeof(uint32_t));
    float value = net_to_float(net_value);

    /* -------------------------
       Reverse lookup IP → hostname
    -------------------------- */
    char hostbuf[NI_MAXHOST];
    if (getnameinfo((struct sockaddr*)&server_addr, server_len,
                    hostbuf, sizeof(hostbuf), NULL, 0, NI_NAMEREQD) != 0)
    {
        strcpy(hostbuf, server_name);
    }

    /* -------------------------
       Output formattato
    -------------------------- */

    printf("Ricevuto risultato dal server %s (ip %s). ",
            hostbuf, server_ip_str);

    if (status == STATUS_SUCCESS) {
        if (rtype=='t')
            printf("%s: Temperatura = %.1f°C\n", city, value);
        else if (rtype=='h')
            printf("%s: Umidità = %.1f%%\n", city, value);
        else if (rtype=='w')
            printf("%s: Vento = %.1f km/h\n", city, value);
        else if (rtype=='p')
            printf("%s: Pressione = %.1f hPa\n", city, value);
    }
    else if (status == STATUS_CITY_NOT_FOUND) {
        printf("Città non disponibile\n");
    }
    else if (status == STATUS_INVALID_REQ) {
        printf("Richiesta non valida\n");
    }

    closesocket(my_socket);

    printf("Client terminato.\n");

    clearwinsock();
    return 0;
}
