#define _POSIX_C_SOURCE 200809L

#include <ftw.h>
#include <signal.h>
#include <sys/select.h>
#include <pthread.h>

#include "headers/sktcomm.h"
#include "headers/serverfuns.h"
#include "headers/utils.h"

/*
  Funzione del thread dedicato alle richieste del client, prende in input il
  file descriptor inizializzato dal main a seguito dell'accept.
*/
void worker(void *client_skt) {
  int fd_client, fd_ind_max, terminated = 0;
  char *dir_pathname = NULL, *client_name = NULL;
  size_t dir_pathdim;
  fd_set set;

  fd_client = *(int *)client_skt;

  // preparo maschera per select
  FD_ZERO(&set);
  FD_SET(fd_client, &set);
  FD_SET(notify_pipe[0], &set);

  // ottengo il massimo tra fd del client e fd pipe di notifica
  if (fd_client > notify_pipe[0])
    fd_ind_max = fd_client;
  else
    fd_ind_max = notify_pipe[0];

  while (!terminated) {
    char *h_type, *save, *buf_request = NULL, *buf_response = NULL;
    size_t nbytes;
    int ok = 0;
    fd_set read_fds = set;

    // attendo su fd del client e pipe di notifica
    SYSCALL(select(fd_ind_max + 1, &read_fds, NULL, NULL, NULL), "select worker",
                   goto cleanup);

    if (FD_ISSET(notify_pipe[0], &read_fds))
      // caso richiesta terminazione a seguito di un segnale
      goto cleanup;
    else if (FD_ISSET(fd_client, &read_fds)) {
      // caso richiesta dal client
      int status;

      // leggo l'header dal socket fino al newline
      status = read_header(fd_client, &buf_request);
      if (status == 0)
        // client ha terminato connessione
        goto cleanup;
      else if (status == -1) {
        // altri errori
        perror("read header");
        goto cleanup;
      }
    }

    // tipo di richiesta che il client vuole
    h_type = strtok_r(buf_request, " \n", &save);

    if (strcmp(h_type, REGISTER_STR) == 0) {
      char *c_name;

      // ottengo il nome del client, per poter creare la sua directory
      c_name = strtok_r(NULL, " \n", &save);
      LIBCALL(client_name = strdup(c_name), "strdup", goto cleanup);

      // creo la directory dedicata al client
      if ((dir_pathname = create_client_dir(c_name, &dir_pathdim)) == NULL) {
        // operazione fallita
        ok = 0;
        terminated = 1;
      }
      else
        // operazione completata
        ok = 1;
    }
    else if (strcmp(h_type, STORE_STR) == 0) {
      char *file_name, *data;
      size_t len;
      ssize_t status;

      // client non si è registrato
      if (client_name == NULL)
        goto resp;

      // ottengo il nome del file da creare
      file_name = strtok_r(NULL, " \n", &save);
      // ottengo la lunghezza dei dati
      sscanf(strtok_r(NULL, " \n", &save), "%zu", &len);
      // leggo la parte dati, è incluso anche uno spazio dopo il newline.
      LIBCALL(data = (char *)calloc(len + 1, sizeof(char)),
              "calloc data_server", goto cleanup);
      status = readn(fd_client, data, (len + 1) * sizeof(char));

      if (status == 0)
        // se client ha terminato connessione, esco
        goto cleanup;
      else if (status == -1) {
        // problema del server, operazione fallita
        perror("read data_server");
        goto cleanup;
      }
      else {
        size_t old_size;

        /*
          creo il file del client, tenendo conto dello spazio tra il newline e i
          dati; se il file già esiste, ricevo la vecchia dimensione
        */
        if ((old_size = create_client_file(file_name, len, data, dir_pathname, dir_pathdim)) == -1)
          // operazione fallita
          ok = 0;
        else {
          // operazione completata
          ok = 1;

          // aggiorno contatori
          PTHCALL(pthread_mutex_lock(&server_stats.stats_lock), "lock store", goto cleanup);
            if (old_size == 0) {
              // file non esisteva
              server_stats.n_obj++;
              server_stats.obj_store_size += len;
            }
            else
              // file esisteva
              server_stats.obj_store_size += len - old_size;
          PTHCALL(pthread_mutex_unlock(&server_stats.stats_lock), "unlock store", goto cleanup);
        }

        free(data); // cleanup buffer usato per leggere i dati
      }
    }
    else if (strcmp(h_type, RETRIEVE_STR) == 0) {
      char *file_name, *data;
      size_t len;

      // client non si è registrato
      if (client_name == NULL)
        goto resp;

      // ottengo il nome del file
      file_name = strtok_r(NULL, " \n", &save);
      // ottengo i dati del file, se esiste, dalla cartella del client
      if ((data = retrieve_client_file(file_name, &len, dir_pathname, dir_pathdim)) == NULL)
        // operazione fallita
        ok = 0;
      else {
        // operazione completata
        if ((buf_response = init_header(DATA_STR, &nbytes, len, data)) == NULL) {
          // problema del server
          perror("init response data");
          goto cleanup;
        }
        ok = 2;
        free(data); // cleanup buffer usato per leggere i dati
      }
    }
    else if (strcmp(h_type, DELETE_STR) == 0) {
      char *file_name;
      size_t file_size;

      // client non si è registrato
      if (client_name == NULL)
        goto resp;

      // ottengo il nome del file da cancellare
      file_name = strtok_r(NULL, " \n", &save);
      // cancello il file, se esiste
      if ((file_size = delete_client_file(file_name, dir_pathname, dir_pathdim)) == -1)
        // operazione fallita
        ok = 0;
      else {
        // operazione completata
        ok = 1;

        // aggiorno contatori
        PTHCALL(pthread_mutex_lock(&server_stats.stats_lock), "lock delete", goto cleanup);
          server_stats.n_obj--;
          server_stats.obj_store_size -= file_size;
        PTHCALL(pthread_mutex_unlock(&server_stats.stats_lock), "unlock delete", goto cleanup);
      }
    }
    else if (strcmp(h_type, LEAVE_STR) == 0) {
      // ha sempre successo
      ok = 1;
      terminated = 1;
    }

    resp:
      if (ok == 1) {
        if ((buf_response = init_header(OK_STR, &nbytes)) == NULL) {
          // problema del server
          perror("init response ok");
          goto cleanup;
        }
      }
      else if (ok == 0) {
        if ((buf_response = init_header(KO_STR, &nbytes, errno)) == NULL) {
          // problema del server
          perror("init response ko");
          goto cleanup;
        }
      }

      // scrivo la risposta
      SYSCALL(writen(fd_client, buf_response, nbytes * sizeof(char)),
              "write response", goto cleanup);

      free(buf_request);    // cleanup buffer lettura richiesta
      free(buf_response);   // cleanup buffer scrittura risposta
      continue;

    cleanup:
      // cleanup e terminazione
      if (buf_request != NULL)
        free(buf_request);
      if (buf_response != NULL)
        free(buf_response);
      terminated = 1;
  }

  if (dir_pathname != NULL)
    free(dir_pathname);
  if (client_name != NULL)
    free(client_name);
  SYSCALL(close(fd_client), "close fd client", ;);
  free(client_skt);

  // diminuisco contatore client
  PTHCALL(pthread_mutex_lock(&server_stats.stats_lock), "lock n_client++", ;);
          server_stats.n_client_conn--;
          if (server_stats.n_client_conn == 0)
            PTHCALL(pthread_cond_signal(&server_stats.exit_cond), "signal cv", ;);
  PTHCALL(pthread_mutex_unlock(&server_stats.stats_lock), "unlock n_client--", ;);
}
