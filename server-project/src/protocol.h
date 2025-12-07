/*
 * protocol.h
 *
 * Header condiviso per client e server meteo
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stddef.h>

// -------------------------------
// Parametri generali del protocollo
// -------------------------------
#define SERVER_PORT     56700    // Porta di default
#define BUFFER_SIZE     512
#define QUEUE_SIZE      5        // Connessioni pendenti in ascolto
#define CITY_NAME_LEN   64       // Lunghezza massima del nome città

// -------------------------------
// Codici di stato della risposta
// -------------------------------
#define STATUS_SUCCESS        0   // Richiesta valida
#define STATUS_CITY_NOT_FOUND 1   // Città non disponibile
#define STATUS_INVALID_REQ    2   // Tipo non valido

// -------------------------------
// Strutture del protocollo
// -------------------------------

// Messaggio di richiesta (Client → Server)
typedef struct {
    char type;                   // 't', 'h', 'w', 'p'
    char city[CITY_NAME_LEN];    // Nome città (stringa terminata da '\0')
} weather_request_t;

// Messaggio di risposta (Server → Client)
typedef struct {
    unsigned int status;         // Codice di stato
    char type;                   // Eco del tipo richiesto oppure '\0' in caso di errore
    float value;                 // Valore meteo (solo se status = 0)
} weather_response_t;

// -------------------------------
// Prototipi funzioni meteo
// (devono restituire float entro range realistici)
// -------------------------------
float get_temperature(void); // -10.0 .. 40.0 °C
float get_humidity(void);    // 20.0 .. 100.0 %
float get_wind(void);        // 0.0 .. 100.0 km/h
float get_pressure(void);    // 950.0 .. 1050.0 hPa

// -------------------------------
// Funzioni AGGIUNTE per la validazione
// -------------------------------

// Verifica se il type è valido: 't','h','w','p'
int valida_tipo(char t);

// Verifica se la città è supportata (case-insensitive)
int citta_supportata(const char *city);

#endif /* PROTOCOL_H_ */
