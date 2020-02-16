#ifndef OSCLIENT_H
#define OSCLIENT_H

extern char err_msg[50];

int os_connect(char *name);

int os_store(char *name, void *block, size_t len);

void *os_retrieve(char *name);

int os_delete(char *name);

int os_disconnect();

#endif
