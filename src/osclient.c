#define _POSIX_C_SOURCE 200809L

#include "headers/sktcomm.h"
#include "headers/osclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

static char *check_response(char *buffer, ...);

int client_skt = -1;
char err_msg[50] = "";

/*
  @requires: name != NULL
  @effects: inizia la connessione all'object store, registrando il client con
    l'identificatore "name" dato. In caso il client fosse già registrato fallisce.
  @returns: true se connessione riuscita, false altrimenti.
*/
int os_connect(char *name){
  struct sockaddr_un skt_address;
  char *buf_request = NULL, *buf_response = NULL, *msg = NULL;
  size_t nbytes;
  ssize_t status;
  int tries = 10;

  // controllo correttezza degli argomenti
  if (name == NULL) {
    errno = EINVAL;
    goto error;
  }

  // controllo se il client era già connesso
  if (client_skt != -1) {
    sprintf(err_msg, "%s", ERRACONN);
    return 0;
  }

  // costruisco l'indirizzo del socket
  memset(&skt_address, 0, sizeof(struct sockaddr_un));
  skt_address.sun_family = AF_UNIX;
  strncpy(skt_address.sun_path, SOCK_PATH, SOCK_PATH_MAX);

  if ((client_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    goto error;

  // tento la connessione per un massimo di 10 volte
  for (int i = 0; i <= tries; i++) {
    if ((connect(client_skt, (struct sockaddr *)&skt_address,
                 sizeof(struct sockaddr_un))) == -1) {
      // connessione fallita
      if (i == tries || (errno != ETIMEDOUT && errno != ENOENT))
        goto error;
      else if (errno == ETIMEDOUT || errno == ENOENT) {
        errno = 0;
        sleep(2);
      }
    }
    else
      // connessione riuscita
      break;
  }

  // creo l'header di richiesta registrazione
  if((buf_request = init_header(REGISTER_STR, &nbytes, name)) == NULL)
    goto error;

  // invio richiesta
  if (writen(client_skt, buf_request, nbytes) <= 0)
    goto error;

  // ricevo la risposta
  if ((status = read_header(client_skt, &buf_response)) <= 0)
    goto error;

  // controllo la risposta
  if ((msg = check_response(buf_response)) != NULL) {
    strcpy(err_msg, msg);
    free(msg);
    goto error;
  }

  free(buf_request);  // cleanup buffer usato per scrittura richiesta
  free(buf_response); // cleanup buffer usato per lettura risposta.
  return 1;

  error:
    if (buf_request != NULL)
      free(buf_request);
    if (buf_response != NULL)
      free(buf_response);
    close(client_skt);
    client_skt = -1;
    if (errno != 0) {
      msg = strerror(errno);
      strcpy(err_msg, msg);
    }
    return 0;
}

/*
  @requires: name != NULL, block != NULL, len != 0.
  @effects: se il client risulta registrato, richiede memorizzazione con nome "name"
    dell'oggetto puntato da "block" di lunghezza "len".
  @returns: true se memorizzazione riuscita, false altrimenti.
*/
int os_store(char *name, void *block, size_t len){
  char *buf_request = NULL, *buf_response = NULL, *data = NULL, *msg = NULL;
  size_t nbytes;
  ssize_t status;

  // controllo se il client si era registrato
  if (client_skt == -1) {
    sprintf(err_msg, "%s", ERRNCONN);

    return 0;
  }

  // controllo correttezza degli argomenti
  if (name == NULL || block == NULL || len == 0){
    errno = EINVAL;
    goto error;
  }

  data = (char *)block;

  // creo l'header di richiesta store
  if((buf_request = init_header(STORE_STR, &nbytes, name, len, data)) == NULL)
    goto error;

  // invio richiesta
  if ((status = writen(client_skt, buf_request, nbytes)) <= 0)
    goto error;

  // ricevo la risposta
  if ((status = read_header(client_skt, &buf_response)) <= 0)
    goto error;

  // controllo la risposta
  if ((msg = check_response(buf_response)) != NULL) {
    strcpy(err_msg, msg);
    free(msg);
    goto error;
  }

  free(buf_request);  // cleanup buffer usato per scrittura richiesta
  free(buf_response); // cleanup buffer usato per lettura risposta

  return 1;

  error:
    if (buf_request != NULL)
      free(buf_request);
    if (buf_response != NULL)
      free(buf_response);
    if (errno != 0) {
      msg = strerror(errno);
      strcpy(err_msg, msg);
    }
    if (status == 0) {
      close(client_skt);
      client_skt = -1;
    }

    return 0;
}

/*
  @requires: name != NULL.
  @effects: se il client risulta registrato, richiede recupero all'obj store dell'oggetto memorizzato
    con il nome "name".
  @returns: puntatore a un blocco di memoria contenente i dati se recupero riuscito,
    NULL altrimenti.
*/
void *os_retrieve(char *name) {
  char *buf_request = NULL, *buf_response = NULL, *data = NULL, *msg = NULL;
  size_t nbytes, len;
  ssize_t status;

  // controllo se il client si era registrato
  if (client_skt == -1) {
    sprintf(err_msg, "%s", ERRNCONN);

    return 0;
  }

  // controllo correttezza degli argomenti
  if (name == NULL) {
    errno = EINVAL;
    goto error;
  }

  // creo l'header di richiesta retrieve
  if ((buf_request = init_header(RETRIEVE_STR, &nbytes, name)) == NULL)
    goto error;

  // invio richiesta
  if ((status = writen(client_skt, buf_request, nbytes * sizeof(char))) <= 0)
    goto error;

  // ricevo la risposta, solo la parte fino a newline
  if ((status = read_header(client_skt, &buf_response)) <= 0)
    goto error;

  // controllo la risposta
  if ((msg = check_response(buf_response, &len)) != NULL) {
    strcpy(err_msg, msg);
    free(msg);
    goto error;
  }

  free(buf_response);   // cleanup buffer usato per lettura risposta

  // leggo la parte dati
  if ((buf_response = (char *)calloc(len + 1, sizeof(char))) == NULL)
    goto error;
  if ((status = readn(client_skt, buf_response, (len + 1) * sizeof(char))) <= 0)
    goto error;

  // ricopio da buf_response[1] per eliminare lo spazio
  if ((data = (char *)calloc(len + 1, sizeof(char))) == NULL)
    goto error;
  memcpy(data, &buf_response[1], len * sizeof(char));

  free(buf_request);  // cleanup buffer usato per scrittura richiesta
  free(buf_response); // cleanup buffer usato per lettura dati

  return (void *)data;

  error:
    if (buf_request != NULL)
      free(buf_request);
    if (buf_response != NULL)
      free(buf_response);
    if (data != NULL)
      free(data);
    if (errno != 0) {
      msg = strerror(errno);
      strcpy(err_msg, msg);
    }
    if (status == 0) {
      close(client_skt);
      client_skt = -1;
    }
    return NULL;
}

/*
  @requires: name != NULL.
  @effects: cancella l'oggetto memorizzato con nome "name". Se client non registrato
    fallisce.
  @returns: true se cancellazione riuscita, false altrimenti.
*/
int os_delete(char *name) {
  char *buf_request = NULL, *buf_response = NULL, *msg = NULL;
  size_t nbytes;
  ssize_t status;

  // controllo se il client era registrato.
  if (client_skt == -1) {
    sprintf(err_msg, "%s", ERRNCONN);

    return 0;
  }

  // controllo correttezza degli argomenti.
  if(name == NULL){
    errno = EINVAL;

    goto error;
  }

  // creo l'header di richiesta delete
  if((buf_request = init_header(DELETE_STR, &nbytes, name)) == NULL)
    goto error;

  // invio richiesta
  if ((status = writen(client_skt, buf_request, nbytes)) <= 0)
    goto error;

  // ricevo la risposta
  if ((status = read_header(client_skt, &buf_response)) <= 0)
    goto error;

  // controllo la risposta
  if ((msg = check_response(buf_response)) != NULL) {
    strcpy(err_msg, msg);
    free(msg);
    goto error;
  }

  free(buf_request);   // cleanup buffer usato per scrittura richiesta
  free(buf_response);  // cleanup buffer usato per lettura risposta

  return 1;

  error:
    if (buf_request != NULL)
      free(buf_request);
    if (buf_response != NULL)
      free(buf_response);
    if (errno != 0) {
      msg = strerror(errno);
      strcpy(err_msg, msg);
    }
    if (status == 0) {
      close(client_skt);
      client_skt = -1;
    }
    return 0;
}

/*
  @effects: chiude connessione all'os. Se client non registrato fallisce.
  @returns: true se disconnessione riuscita, false altrimenti.
*/
int os_disconnect() {
  char *buf_request = NULL, *buf_response = NULL, *msg;
  size_t nbytes;
  ssize_t status;

  if (client_skt == -1) {
    sprintf(err_msg, "%s", ERRNCONN);

    return 0;
  }

  // creo l'header di richiesta leave
  if((buf_request = init_header(LEAVE_STR, &nbytes)) == NULL)
    goto error;

  // invio richiesta
  if ((status = writen(client_skt, buf_request, nbytes * sizeof(char))) <= 0)
    goto error;

  // ricevo la risposta
  if ((status = read_header(client_skt, &buf_response)) <= 0)
    goto error;

  // controllo la risposta
  if ((msg = check_response(buf_response)) != NULL) {
    strcpy(err_msg, msg);
    free(msg);
    goto error;
  }

  free(buf_request);    // cleanup buffer usato per scrittura richiesta
  free(buf_response);   // cleanup buffer usato per lettura risposta
  close(client_skt);
  client_skt = -1;
  return 1;

  error:
    if (buf_request != NULL)
      free(buf_request);
    if (buf_response != NULL)
      free(buf_response);
    if (errno != 0) {
      msg = strerror(errno);
      strcpy(err_msg, msg);
    }
    close(client_skt);
    client_skt = -1;
    return 0;
}

/*
  Controlla la risposta ricevuta dal server, in caso di OK ritorna NULL, in caso di
  KO ritorna il messaggio di errore spedito dal server, in caso di DATA inizializza
  la variabile len passata come argomento e ritorna NULL
*/
static char *check_response(char *buffer, ...){
  char *token;
  va_list arg_list;

  va_start(arg_list, buffer);

  token = strtok(buffer, " \n");
  if (strcmp(token, OK_STR) == 0)
    return NULL;
  else if (strcmp(token, KO_STR) == 0) {
    char *msg;

    token = strtok(NULL, "\n");
    // copio il messaggio senza considerare l'ultimo spazio prima del newline
    msg = strndup(token, strlen(token)-1);

    return msg;
  }
  else if (strcmp(token, DATA_STR) == 0) {
    size_t *len = va_arg(arg_list, size_t *);

    token = strtok(NULL, " \n");
    sscanf(token, "%zu", len);

    return NULL;
  }

  va_end(arg_list);

  return NULL;
}
