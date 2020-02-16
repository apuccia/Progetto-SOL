#define _POSIX_C_SOURCE 200809L

#include <ftw.h>
#include <signal.h>
#include <sys/select.h>
#include <pthread.h>

#include "headers/serverfuns.h"
#include "headers/sktcomm.h"
#include "headers/utils.h"

// argomenti thread handler
typedef struct sighandler_args {
  int       fd;
  sigset_t  set;
} sighandler_args;

int counter(const char *file_path, const struct stat *sb, int flag);
void sighandler(void *arg);
int spawn_thread(void * (*fun)(void *), void *arg);

void worker(void *client_skt);

// variabili globali
stats server_stats;
int notify_pipe[2];

int main() {
  int fd_server, fd_ind_max, terminated = 0;
  struct sockaddr_un skt_address;
  fd_set set;
  struct sigaction sig_s;
  sigset_t sig_set;
  sighandler_args arg;
  pthread_t th_handler;
  pthread_attr_t th_handler_attr;

  // costruisco maschera segnali
  sigemptyset(&sig_set);
  sigaddset(&sig_set, SIGINT);
  sigaddset(&sig_set, SIGQUIT);
  sigaddset(&sig_set, SIGTERM);
  sigaddset(&sig_set, SIGUSR1);

  // blocco tutti i segnali specificati nella maschera
  PTHCALL(pthread_sigmask(SIG_BLOCK, &sig_set, NULL), "sigmask", goto error);

  // ignoro sigpipe
  memset(&sig_s, 0, sizeof(struct sigaction));
  sig_s.sa_handler = SIG_IGN;
  SYSCALL(sigaction(SIGPIPE, &sig_s, NULL), "sigaction", goto error);

  // pipe per notifiche segnali
  SYSCALL(pipe(notify_pipe), "pipe init", goto error);

  // inizializzo argomenti del thread gestore segnali
  arg.fd = notify_pipe[1];
  arg.set = sig_set;

  // inizializzazione struttura statistiche
  memset(&server_stats, 0, sizeof(stats));
  PTHCALL(pthread_mutex_init(&server_stats.stats_lock, NULL), "lock init",
          goto error);
  PTHCALL(pthread_cond_init(&server_stats.exit_cond, NULL), "cond init", goto error);

  /*
    creo directory principale
    path: ./data
  */
  if (mkdir(DATA_PATH, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
    if (errno != EEXIST) {
      // altri errori
      perror("mkdir data");

      goto error;
    }
    else
      // controllo iniziale per capire il numero e il peso degli oggetti già presenti
      SYSCALL(ftw(DATA_PATH, counter, 2), "ftw", goto error);
  }

  // spawn thread gestore segnali
  PTHCALL(pthread_attr_init(&th_handler_attr), "pth_attr_init", return -1);
  PTHCALL(pthread_create(&th_handler, &th_handler_attr, (void *)sighandler, &arg),
                         "create thread sighandler", return -1);

  // costruisco indirizzo socket listener
  memset(&skt_address, 0, sizeof(struct sockaddr_un));
  skt_address.sun_family = AF_UNIX;
  strncpy(skt_address.sun_path, SOCK_PATH, SOCK_PATH_MAX);

  // inizializzo il fd del socket e lo rendo passivo (listener)
  SYSCALL(fd_server = socket(AF_UNIX, SOCK_STREAM, 0), "socket server",
          goto error);
  SYSCALL(bind(fd_server, (struct sockaddr *)&skt_address,
               sizeof(struct sockaddr_un)),
          "bind server", goto error);
  SYSCALL(listen(fd_server, SOMAXCONN), "listen", goto error);

  // costruisco maschera per select
  FD_ZERO(&set);
  FD_SET(fd_server, &set);
  FD_SET(notify_pipe[0], &set);

  // ottengo il massimo tra fd passivo e fd pipe lato lettura
  if (fd_server > notify_pipe[0])
    fd_ind_max = fd_server;
  else
    fd_ind_max = notify_pipe[0];

  while (!terminated) {
    fd_set read_fds = set;

    SYSCALL(select(fd_ind_max + 1, &read_fds, NULL, NULL, NULL), "select main",
                   goto error);

    if (FD_ISSET(notify_pipe[0], &read_fds))
      // caso ricezione segnale SIGQUIT, SIGSEGV, SIGTERM, SIGINT
      terminated = 1;
    else if (FD_ISSET(fd_server, &read_fds)) {
      // nuovo client
      int *fd_client = (int *)malloc(sizeof(int));

      SYSCALL(*fd_client = accept(fd_server, NULL, NULL), "accept",
              free(fd_client); goto error);

      // aggiorno contatori
      PTHCALL(pthread_mutex_lock(&server_stats.stats_lock), "lock n_client++",
              goto error);
        server_stats.n_client_conn++;
        if (server_stats.n_client_conn > server_stats.max_clients_conn)
          server_stats.max_clients_conn = server_stats.n_client_conn;
      PTHCALL(pthread_mutex_unlock(&server_stats.stats_lock), "unlock n_client++",
              goto error);

      // spawn dei worker in modo detached
      if ((spawn_thread((void *)worker, fd_client)) == -1) {
        free(fd_client);
        goto error;
      }
    }
  }

  // aggiorno contatori, distruggo lock e cv
  PTHCALL(pthread_mutex_lock(&server_stats.stats_lock), "lock exit", goto error);
    while (server_stats.n_client_conn > 0)
      // aspetto che tutti i thread worker terminino
      PTHCALL(pthread_cond_wait(&server_stats.exit_cond, &server_stats.stats_lock),
              "wait exit", goto error);
    PTHCALL(pthread_cond_destroy(&server_stats.exit_cond), "destroy cv", goto error);
  PTHCALL(pthread_mutex_unlock(&server_stats.stats_lock), "unlock exit", goto error);

  // cleanup
  PTHCALL(pthread_mutex_destroy(&server_stats.stats_lock), "destroy lock", goto error);
  PTHCALL(pthread_join(th_handler, NULL), "join th handler", goto error);
  SYSCALL(unlink(SOCK_PATH), "unlink sock", goto error);
  SYSCALL(close(notify_pipe[0]), "close notify_pipe[0]", goto error);

  return 0;

  error:
    unlink(SOCK_PATH);
    return -1;
}

/*
  Funzione del thread dedicato alla gestione dei segnali, in caso di SIGUSR1
  stampa le statistiche del server, in caso di SIGINT, SIGSEGV, SIGQUIT, SIGTERM
  o a seguito di errori, chiude il descrittore lato scrittura della pipe per
  risvegliare il thread main e i thread worker in attesa sulla select.
*/
void sighandler(void *arg) {
  sigset_t set;
  int fd_pipe, terminated = 0;

  set = ((sighandler_args *)arg)->set;
  fd_pipe = ((sighandler_args *)arg)->fd;

  while (!terminated) {
    int sig;

    // mi metto in attesa sui segnali specificati dalla maschera
    if (sigwait(&set, &sig) != 0) {
      perror("sigwait");
      goto error;
    }

    switch (sig) {
      case SIGUSR1: {
        printf("\n********** SERVER STATS **********\n");
        PTHCALL(pthread_mutex_lock(&server_stats.stats_lock), "lock print_stats", goto error);
          printf("--Numero di client connessi: %d--\n", server_stats.n_client_conn);
          printf("--Massimo numero di client connessi: %d--\n", server_stats.max_clients_conn);
          printf("--Numero di oggetti contenuti nell'obj store: %d--\n", server_stats.n_obj);
          printf("--Dimensione totale obj store: %zu--\n", server_stats.obj_store_size);
        PTHCALL(pthread_mutex_unlock(&server_stats.stats_lock), "unlock print_stats", goto error);

        break;
      }
      case SIGQUIT:
      case SIGTERM:
      case SIGSEGV:
      case SIGINT: {
        terminated = 1;
        SYSCALL(close(fd_pipe), "close fd_pipe", continue);
        continue;
      }

      error:
        close(fd_pipe);
        terminated = 1;
    }
  }
}

/*
  Ad ogni avvio del server, se è presente la directory data, inizializza i
  contatori delle statistiche in base ai file già presenti.
*/
int counter(const char *file_path, const struct stat *sb, int flag) {
  if (flag == FTW_F) {
      server_stats.n_obj++;
      server_stats.obj_store_size += sb->st_size;
  }

  return 0;
}

/*
  Spawna i thread in modalità detached passando come argomenti la funzione da
  eseguire e gli argomenti della funzione.
*/
int spawn_thread(void * (*fun)(void *), void *arg) {
  pthread_t th;
  pthread_attr_t th_attr;

  // spawn thread in modalità detached
  PTHCALL(pthread_attr_init(&th_attr), "pth_attr_init", return -1);
  PTHCALL(pthread_attr_setdetachstate(&th_attr, PTHREAD_CREATE_DETACHED),
          "set_detach",
          return -1);
  PTHCALL(pthread_create(&th, &th_attr, fun, arg), "create_thread", return -1);

  return 0;
}
