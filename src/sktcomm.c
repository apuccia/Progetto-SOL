#include "headers/sktcomm.h"

ssize_t readn(int fd, void *block, size_t size) {
  size_t left = size;
  int r;
  char *buffer = (char *)block;

  while (left > 0) {
    if ((r = read(fd, buffer, left)) == -1) {
      if (errno == EINTR)
        continue;

      // altri errori
      return -1;
	  }
    else if (r == 0)
      // connessione terminata dall'altro lato della comunicazione
      return 0;

    left -= r;
	  buffer += r;
  }

  return size;
}

ssize_t writen(int fd, void *block, size_t size) {
  size_t left = size;
  int r;
  char *buffer = (char *)block;

  while (left > 0) {
    if ((r = write(fd, buffer, left)) == -1) {
      if (errno == EINTR)
        continue;

      // altri errori
	    return -1;
	  }
	  else if (r == 0)
      // connessione terminata dall'altro lato della comunicazione
      return 0;

    left -= r;
	  buffer += r;
  }

  return size;
}

/*
  Funzione che inizializza l'header di richiesta/risposta, viene utilizzata
  sia dal client che dal server, ritorna NULL se h_type non è riconosciuto oppure
  a seguito di errori dovuti a calloc
*/
char *init_header(char *h_type, size_t *nbytes, ...) {
  va_list arg_list;
  char *buffer = NULL;

  va_start(arg_list, nbytes);

  if (strcmp(h_type, REGISTER_STR) == 0) {
    char *name = va_arg(arg_list, char *);

    /*
      costruisco header di richiesta: REGISTER nome_client \n
      snprintf mi da il numero di bytes scritti non considerando il terminatore
    */
    *nbytes = strlen(name) + HEADER_BASE_MAX;
    if ((buffer = (char *)calloc(*nbytes + 1, sizeof(char))) == NULL)
      goto error;

    *nbytes = snprintf(buffer, (*nbytes + 1) * sizeof(char), "%s %s \n", h_type, name);
  }
  else if (strcmp(h_type, STORE_STR) == 0) {
    char *name = va_arg(arg_list, char *);
    size_t len = va_arg(arg_list, size_t);
    char *data = va_arg(arg_list, char *);

    /*
      costruisco header di richiesta: STORE nome_file len \n data
      snprintf mi da il numero di bytes scritti non considerando il terminatore
    */
    *nbytes = strlen(name) + len + HEADER_BASE_MAX + sizeof(size_t);
    if ((buffer = (char *)calloc(*nbytes + 1, sizeof(char))) == NULL)
      goto error;

    *nbytes = snprintf(buffer, (*nbytes + 1) * sizeof(char), "%s %s %zu \n ",
                       h_type, name, len);
    *nbytes += len;
    strcat(buffer, data);
  }
  else if (strcmp(h_type, RETRIEVE_STR) == 0) {
    char *name = va_arg(arg_list, char *);

    /*
      costruisco header di richiesta: RETRIEVE nome_file \n
      snprintf mi da il numero di bytes scritti non considerando il terminatore
    */
    *nbytes = strlen(name) + HEADER_BASE_MAX;
    if ((buffer = (char *)calloc(*nbytes + 1, sizeof(char))) == NULL)
      goto error;

    *nbytes = snprintf(buffer, (*nbytes + 1) * sizeof(char), "%s %s \n", h_type, name);
  }
  else if (strcmp(h_type, DELETE_STR) == 0) {
    char *name = va_arg(arg_list, char *);

    /*
      costruisco header di risposta: DELETE nome_file \n
      snprintf mi da il numero di bytes scritti non considerando il terminatore
    */
    *nbytes = strlen(name) + HEADER_BASE_MAX;
    if ((buffer = (char *)calloc(*nbytes + 1, sizeof(char))) ==  NULL)
      goto error;

    *nbytes = snprintf(buffer, (*nbytes + 1) * sizeof(char), "%s %s \n", h_type, name);
  }
  else if (strcmp(h_type, LEAVE_STR) == 0) {
    /*
      costruisco header di richiesta: LEAVE \n
      snprintf mi da il numero di bytes scritti non considerando il terminatore
    */

    *nbytes = HEADER_BASE_MAX;
    if ((buffer = (char *)calloc(*nbytes + 1, sizeof(char))) == NULL)
      goto error;

    *nbytes = snprintf(buffer, (*nbytes + 1) * sizeof(char), "%s \n", h_type);
  }
  else if (strcmp(h_type, OK_STR) == 0) {
    /*
      costruisco header di risposta: OK \n.
      snprintf mi da il numero di bytes scritti non considerando il terminatore
    */
    *nbytes = HEADER_BASE_MAX;
    if ((buffer = (char *)calloc(*nbytes + 1, sizeof(char))) == NULL)
      goto error;

    *nbytes = snprintf(buffer, (*nbytes + 1) * sizeof(char), "%s \n", OK_STR);
  }
  else if (strcmp(h_type, KO_STR) == 0) {
    int err = va_arg(arg_list, int);
    char *msg;

    switch (err) {
      case ENOENT:
        msg = ERRNEX;
        break;
      case ENAMETOOLONG:
        msg = ERRNAME;
        break;
      default:
        msg = ERRINTERNAL;
    }

    /*
      costruisco header di risposta: KO msg \n.
      snprintf mi da il numero di bytes scritti non considerando il terminatore
    */
    *nbytes = sizeof(int) + HEADER_BASE_MAX + strlen(msg);
    if ((buffer = (char *)calloc(*nbytes + 1, sizeof(char))) == NULL)
      goto error;

    *nbytes = snprintf(buffer, (*nbytes + 1) * sizeof(char), "%s %s \n", KO_STR,
                       msg);
  }
  else if (strcmp(h_type, DATA_STR) == 0) {
    size_t len = va_arg(arg_list, size_t);
    char *data = va_arg(arg_list, char *);

    *nbytes = HEADER_BASE_MAX + len + sizeof(size_t);

    /*
      costruisco header di risposta: DATA len \n data.
      snprintf mi da il numero di bytes scritti non considerando il terminatore
    */
    if ((buffer = (char *)calloc(*nbytes + 1, sizeof(char))) == NULL)
      goto error;

    *nbytes = snprintf(buffer, (*nbytes + 1) * sizeof(char) , "%s %zu \n ",
                       DATA_STR, len);
    *nbytes += len;
    strcat(buffer, data);
  }

  va_end(arg_list);
  return buffer;

  error:
    va_end(arg_list);
    return NULL;
}

/*
  Legge dal socket l'header fino al carattere newline, fallisce in caso di
  lettura
*/
int read_header(int fd, char **buffer) {
  size_t initial_dim = BUFF_SIZE;
  int i = 0, skt_opened = 1;

  if ((*buffer = (char *)malloc(initial_dim * sizeof(char))) == NULL)
    return -1;

  do {
    if (i == initial_dim - 1) {
      // se l'header fino a newline è più grande di BUFF_SIZE (512) lo rialloco
      char *new_ptr;

      initial_dim = 2 * initial_dim;
      if ((new_ptr = (char *)realloc(*buffer, initial_dim * sizeof(char))) == NULL)
        goto error;

      *buffer = new_ptr;
    }

    // leggo un byte alla volta fino al carattere newline.
    if ((skt_opened = readn(fd, &((*buffer)[i]), sizeof(char))) == -1)
      goto error;

  } while (skt_opened && (*buffer)[i++] != '\n');

  // il client ha terminato la connessione.
  if (skt_opened == 0) {
    free(*buffer);
    *buffer = NULL;
    return 0;
  }

  // aggiungo il terminatore, nell'header non è previsto.
  (*buffer)[i] = '\0';

  return 1;

  error:
    free(*buffer);
    *buffer = NULL;
    return -1;
}
