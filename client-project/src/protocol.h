/*
 * protocol.h
 *
 * Header condiviso per client meteo
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stddef.h>

// --- Parametri generali ---
#define SERVER_PORT 56700      // Porta di default del server
#define QUEUE_SIZE 5
#define CITY_NAME_LEN 64       // Lunghezza massima nome città

// --- Codici di stato risposta ---
#define STATUS_SUCCESS 0        // Richiesta valida
#define STATUS_CITY_NOT_FOUND 1 // Città non disponibile
#define STATUS_INVALID_REQ 2    // Tipo non valido

// --- Strutture dei messaggi ---

// Messaggio di richiesta (Client -> Server)
typedef struct {
    char type;                 // Tipo dato: 't', 'h', 'w', 'p'
    char city[CITY_NAME_LEN];  // Nome città (null-terminated)
} weather_request_t;

// Messaggio di risposta (Server -> Client)
typedef struct {
    unsigned int status; // Codice di stato
    char type;           // Eco del tipo richiesto
    float value;         // Valore meteo
} weather_response_t;

#endif /* PROTOCOL_H_ */
