#ifndef SERVERFUNS_H
#define SERVERFUNS_H

#define DATA_PATH "./data"
#define DATA_PATH_SIZE 6

char *create_client_dir(char *client_name, size_t *dir_pathdim);

int create_client_file(char *file_name, size_t file_size, char *data,
                       char *dir_pathname, size_t dir_pathdim);

int delete_client_file(char *file_name, char *dir_pathname, size_t dir_pathdim);

char *retrieve_client_file(char *file_name, size_t *file_size, char *dir_pathname,
                           size_t dir_pathdim);

#endif
