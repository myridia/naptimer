#ifndef NAPTIMER_STORE_H
#define NAPTIMER_STORE_H

#include <stddef.h>
#include "util.h"

typedef struct
{
    void *db;
    char path[NT_PATH_LEN];
} NtStore;

typedef void (*NtStoreCb)(const char *id, const char *rev, const char *doc, void *context);

int nt_store_open(const char *path, NtStore *store, char *error, size_t error_size);
void nt_store_close(NtStore *store);
int nt_store_get(const NtStore *store, const char *id, char **doc, char **rev);
int nt_store_put(NtStore *store, const char *id, const char *doc,
                 char *new_rev, size_t new_rev_size);
int nt_store_del(NtStore *store, const char *id);
int nt_store_each(const NtStore *store, NtStoreCb callback, void *context,
                  char *error, size_t error_size);
int nt_store_backup(const NtStore *store, const char *target, char *error, size_t error_size);
int nt_store_restore(NtStore *store, const char *source, char *error, size_t error_size);
int nt_store_vacuum(NtStore *store, char *error, size_t error_size);

#endif
