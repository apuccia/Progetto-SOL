#ifndef SKTCOMM_H
#define SKTCOMM_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#define SOCK_PATH "./objstore.sock"
#define SOCK_PATH_MAX 108
#define BUFF_SIZE 512

#define REGISTER_STR "REGISTER"
#define STORE_STR "STORE"
#define RETRIEVE_STR "RETRIEVE"
#define DELETE_STR "DELETE"
#define LEAVE_STR "LEAVE"
#define OK_STR "OK"
#define KO_STR "KO"
#define DATA_STR "DATA"

#define HEADER_BASE_MAX 11

#define ERRNCONN "Client not connected"
#define ERRACONN "Client already connected"
#define ERRNEX "File not in object store"
#define ERRNAME "Name chosen too long"
#define ERRINTERNAL "Server internal error"

ssize_t readn(int fd, void *block, size_t size);

ssize_t writen(int fd, void *block, size_t size);

char *init_header(char *h_type, size_t *nbytes, ...);

int read_header(int fd, char **buffer);

#endif
