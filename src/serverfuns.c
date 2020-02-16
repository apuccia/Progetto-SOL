#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

#include "headers/serverfuns.h"

/*
  Crea la directory del client e ritorna il suo path assoluto, inizializzando
  anche la dimensione del path
*/
char *create_client_dir(char *client_name, size_t *dir_pathdim) {
  char *dir_pathname;

  /*
    costruisco il path della cartella del client
    path: ./data/nome_client
  */
  *dir_pathdim = strlen(client_name) + DATA_PATH_SIZE + 2;
  if ((dir_pathname = (char *)calloc(*dir_pathdim, sizeof(char))) == NULL)
    return NULL;
  snprintf(dir_pathname, *dir_pathdim * sizeof(char), "%s/%s",
           DATA_PATH, client_name);

  // creo la cartella del client, a meno che già esista.
  if (mkdir(dir_pathname, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
    if (errno != EEXIST) {
      free(dir_pathname);
      perror("mkdir");
      return NULL;
    }
  }

  return dir_pathname;
}

/*
  Crea il file del client nel path dir_pathname, se esiste un file con lo stesso nome
  ritorna la vecchia dimensione del file, altrimenti ritorna 0. In caso di errori
  ritorna -1
*/
int create_client_file(char *file_name, size_t file_size, char *data,
                       char *dir_pathname, size_t dir_pathdim) {
  char *file_pathname;
  size_t file_pathdim, old_size = 0;
  struct stat file_stat;
  FILE *file;

  /*
    costruisco il path del file che devo conservare
    path: data/nome_client/nome_file.dat
    data/nome_client già inclusi in dir_pathname
  */
  file_pathdim = dir_pathdim + strlen(file_name) + 6;
  if ((file_pathname = (char *)calloc(file_pathdim, sizeof(char))) == NULL)
    return -1;

  snprintf(file_pathname, file_pathdim, "%s/%s.dat", dir_pathname, file_name);

  // se il file già esiste devo capire quanto è grande
  if (stat(file_pathname, &file_stat) != 0) {
    if (errno != ENOENT)
      // altri problemi
      goto error;
    else
      // file non esiste
      old_size = 0;
  }
  else
    old_size = file_stat.st_size;

  // creo il file, se già esiste lo azzero.
  if ((file = fopen(file_pathname, "wb")) == NULL)
    goto error;

  // scrivo i dati, parto da 1 per non scrivere anche lo spazio.
  if (fwrite(&data[1], sizeof(char), file_size, file) != file_size) {
    fclose(file);
    goto error;
  }

  if (fclose(file) == EOF)
    goto error;

  free(file_pathname);  // cleanup buffer contenente il path del file
  return old_size;

  error:
    free(file_pathname);
    return -1;
}

/*
  Elimina il file del client nel path dir_pathname ritornando la sua dimensione,
  se il file non esiste o ci sono altri errori ritorna -1
*/
int delete_client_file(char *file_name, char *dir_pathname, size_t dir_pathdim) {
  size_t file_pathdim;
  char *file_pathname;
  struct stat file_stat;

  /*
    costruisco il path del file che devo eliminare
    path: data/nome_client/nome_file.dat
    data/nome_client già inclusi in dir_pathname
  */
  file_pathdim = dir_pathdim + strlen(file_name) + 6;
  if ((file_pathname = (char *)calloc(file_pathdim, sizeof(char))) == NULL)
    return -1;
  snprintf(file_pathname, file_pathdim * sizeof(char), "%s/%s.dat", dir_pathname, file_name);

  // controllo se il file esiste
  if (stat(file_pathname, &file_stat) == -1)
    goto error;

  // elimino il file
  if (unlink(file_pathname) == -1)
    goto error;

  free(file_pathname);  // cleanup buffer contenente il path del file

  return file_stat.st_size;

  error:
    free(file_pathname);
    return -1;
}

char *retrieve_client_file(char *file_name, size_t *file_size, char *dir_pathname,
                           size_t dir_pathdim) {
  char *file_pathname, *data;
  size_t file_pathdim;
  FILE *file;

  /*
    costruisco il path del file da aprire per recuperare i dati
    path: data/nome_client/nome_file.dat
    data/nome_client già inclusi in dir_pathname
  */
  file_pathdim = strlen(file_name) + dir_pathdim + 6;
  if ((file_pathname = (char *)calloc(file_pathdim, sizeof(char))) == NULL)
    return NULL;
  snprintf(file_pathname, file_pathdim * sizeof(char), "%s/%s.dat", dir_pathname,
           file_name);

  if ((file = fopen(file_pathname, "rb")) == NULL) {
    free(file_pathname);
    return NULL;
  }

  // ottengo la dimensione dei dati contenuti nel file
  if (fseek(file, 0, SEEK_END) == -1)
    goto error;

  if ((*file_size = ftell(file)) == -1)
    goto error;

  rewind(file);

  // leggo i dati
  if ((data = (char *)calloc(*file_size + 1, sizeof(char))) == NULL)
    goto error;

  if (fread(data, sizeof(char), *file_size, file) != *file_size)
    goto err_retr;

  if (fclose(file) == -1)
    goto err_retr;

  free(file_pathname);
  return data;

  err_retr:
    if (data != NULL)
      free(data);
  error:
    free(file_pathname);
    fclose(file);
    return NULL;
}
