#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "headers/osclient.h"

#define TEST_STRING "Questa è una stringa di prova che sarà utilizzata per\
 testare il buon funzionamento dell'obj store"
#define TEST_LEN 100

char *repeat_string(int n);

int main(int argc, char **argv) {
  if(argc != 3) {
    printf("Usage: %s nome numero_test\n", argv[0]);
    return 1;
  }

  int test_type = atoi(argv[2]);
  int op_completed=0;

  if (!os_connect(argv[1]))
    fprintf(stderr, "%s\n", err_msg);

  switch (test_type) {
    case 1: {
      char *data;

      // store primo oggetto da 100 byte
      if (!os_store("Oggetto0", TEST_STRING, TEST_LEN))
        fprintf(stderr, "%s\n", err_msg);
      else
        op_completed++;

      // store degli altri 18 oggetti
      for (int i=1; i<19; i++) {
        char name[10];

        memset(name, 0, 10);
        snprintf(name, 10, "Oggetto%d", i);

        if ((data = repeat_string(i * 55)) == NULL)
          return -1;

        if (!os_store(name, data, i * TEST_LEN * 55))
          fprintf(stderr, "%s\n", err_msg);
        else
          op_completed++;

        free(data);
      }

      // store dell'ultimo oggetto da 100.000 byte
      if ((data = repeat_string(1000)) == NULL)
        return -1;

      if (!os_store("Oggetto19", data, 1000 * TEST_LEN))
        fprintf(stderr, "%s\n", err_msg);
      else
        op_completed++;
      free(data);

      break;
    }
    case 2: {
      char *data;

      /*
        recupero primo oggetto, fallisco se ricevo NULL oppure se la dimensione
        non è concorde a quella della store del Test 1
      */
      if ((data = os_retrieve("Oggetto0")) == NULL)
        fprintf(stderr, "%s\n", err_msg);
      else if (strlen(data) == 100)
        op_completed++;

      if (data != NULL)
        free(data);

      // recupero altri 18 oggetti
      for (int i = 1; i < 19; i++) {
        char name[10];

        memset(name, 0, 10);
        snprintf(name, 10, "Oggetto%d", i);

        if ((data = os_retrieve(name)) == NULL)
          fprintf(stderr, "%s\n", err_msg);
        else if (strlen(data) == (i * 55 * TEST_LEN))
          op_completed++;

        if (data != NULL)
          free(data);
      }

      // recupero ultimo oggetto
      if ((data = os_retrieve("Oggetto19")) == NULL)
        fprintf(stderr, "%s\n", err_msg);
      else if (strlen(data) == 100000)
        op_completed++;

      if (data != NULL)
        free(data);

      break;
    }
    case 3: {
      char name[10];

      // cancello tutti gli oggetti, fallisco se ricevo 0
      for (int i = 0; i < 20; i++) {
        snprintf(name, 10, "Oggetto%d", i);

        if (!os_delete(name))
          fprintf(stderr, "%s\n", err_msg);
        else op_completed++;
      }

      break;
    }
  }

  if (!os_disconnect())
    fprintf(stderr, "%s\n", err_msg);

  // stampa delle statistiche
  fprintf(stdout, "\tNome client: %s, Test %d: ", argv[1], test_type);
  if (op_completed == 20)
    fprintf(stdout, "superato\n");
  else
    fprintf(stdout, "fallito, operazioni fallite: %d\n", 20-op_completed);

  return 0;
}

char *repeat_string(int n_times) {
  char *data;

  if ((data = (char *)calloc((TEST_LEN * n_times) + 1, sizeof(char))) == NULL)
    return NULL;

  for(int i = 0; i<n_times; i++)
    strcpy(data + (TEST_LEN * i), TEST_STRING);

  return data;
}
