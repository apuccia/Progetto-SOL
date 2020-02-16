#ifndef UTILS_H
#define UTILS_H

#define SYSCALL(sc, msg, op) \
  if ((sc) == -1) { \
    perror(msg); \
    op; \
  } \

#define LIBCALL(lc, msg, op) \
  if ((lc) == NULL) { \
    perror(msg); \
    op; \
  } \

#define PTHCALL(pc, msg, op) \
  if((pc) != 0) { \
    perror(msg); \
    op; \
  } \

// statistiche del server
typedef struct stats {
  unsigned int    n_client_conn;
  unsigned int    max_clients_conn;
  unsigned int    n_obj;
  size_t          obj_store_size;
  pthread_mutex_t stats_lock;
  pthread_cond_t  exit_cond;
} stats;

// variabili globali
extern stats server_stats;
extern int notify_pipe[2];

#endif
