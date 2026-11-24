#include "store.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NT_SCHEMA "CREATE TABLE IF NOT EXISTS docs (" \
                   "id TEXT PRIMARY KEY NOT NULL," \
                   "_rev TEXT NOT NULL," \
                   "doc TEXT NOT NULL" \
                   ");"

static void i_set_error(char *error, size_t size, const char *text)
{
    if (error != NULL && size > 0)
        snprintf(error, size, "%s", text);
}

static uint64_t i_hash(const char *text)
{
    uint64_t hash = 1469598103934665603ULL;
    const unsigned char *p;

    for (p = (const unsigned char *)text; p != NULL && *p != '\0'; p += 1)
    {
        hash ^= (uint64_t)*p;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void i_next_rev(const char *old, const char *doc, char *out, size_t size)
{
    unsigned long long counter = 0;
    if (old != NULL)
    {
        const char *dash = strchr(old, '-');
        if (dash != NULL && dash > old)
            counter = strtoull(old, NULL, 10);
    }
    counter += 1;
    snprintf(out, size, "%llu-%016llx", counter,
             (unsigned long long)i_hash(doc));
}

static int i_create_schema(sqlite3 *db, char *error, size_t error_size)
{
    char *message = NULL;
    int rc = sqlite3_exec(db, NT_SCHEMA, NULL, NULL, &message);
    if (rc != SQLITE_OK)
    {
        i_set_error(error, error_size, message != NULL ? message : "Cannot create docs table");
        sqlite3_free(message);
        return -1;
    }
    return 0;
}

int nt_store_open(const char *path, NtStore *store, char *error, size_t error_size)
{
    sqlite3 *db = NULL;
    int rc;

    if (store == NULL || path == NULL)
        return -1;
    memset(store, 0, sizeof(*store));
    rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK)
    {
        i_set_error(error, error_size, db != NULL ? sqlite3_errmsg(db) : "Cannot open database");
        if (db != NULL)
            sqlite3_close(db);
        return -1;
    }
    if (i_create_schema(db, error, error_size) != 0)
    {
        sqlite3_close(db);
        return -1;
    }
    store->db = db;
    snprintf(store->path, sizeof(store->path), "%s", path);
    return 0;
}

void nt_store_close(NtStore *store)
{
    if (store != NULL && store->db != NULL)
    {
        sqlite3_close((sqlite3 *)store->db);
        store->db = NULL;
    }
}

int nt_store_get(const NtStore *store, const char *id, char **doc, char **rev)
{
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    const char *text;
    int rc;

    if (doc != NULL)
        *doc = NULL;
    if (rev != NULL)
        *rev = NULL;
    if (store == NULL || store->db == NULL || id == NULL)
        return -1;
    db = (sqlite3 *)store->db;
    rc = sqlite3_prepare_v2(db, "SELECT doc, _rev FROM docs WHERE id=?1", -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return -1;
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        text = (const char *)sqlite3_column_text(stmt, 0);
        if (doc != NULL && text != NULL)
            *doc = nt_strdup(text);
        text = (const char *)sqlite3_column_text(stmt, 1);
        if (rev != NULL && text != NULL)
            *rev = nt_strdup(text);
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW ? 0 : -1;
}

int nt_store_put(NtStore *store, const char *id, const char *doc,
                 char *new_rev, size_t new_rev_size)
{
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    char *old = NULL;
    char rev[64];
    int rc;
    int result = -1;

    if (store == NULL || store->db == NULL || id == NULL || doc == NULL)
        return -1;
    db = (sqlite3 *)store->db;
    nt_store_get(store, id, NULL, &old);
    i_next_rev(old, doc, rev, sizeof(rev));
    free(old);

    rc = sqlite3_prepare_v2(db,
                            "INSERT INTO docs (id, _rev, doc) VALUES (?1, ?2, ?3) "
                            "ON CONFLICT(id) DO UPDATE SET _rev=excluded._rev, doc=excluded.doc",
                            -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return -1;
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rev, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, doc, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE)
    {
        result = 0;
        if (new_rev != NULL && new_rev_size > 0)
            snprintf(new_rev, new_rev_size, "%s", rev);
    }
    sqlite3_finalize(stmt);
    return result;
}

int nt_store_del(NtStore *store, const char *id)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if (store == NULL || store->db == NULL || id == NULL)
        return -1;
    rc = sqlite3_prepare_v2((sqlite3 *)store->db, "DELETE FROM docs WHERE id=?1", -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return -1;
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int nt_store_each(const NtStore *store, NtStoreCb callback, void *context,
                  char *error, size_t error_size)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if (store == NULL || store->db == NULL || callback == NULL)
        return -1;
    rc = sqlite3_prepare_v2((sqlite3 *)store->db, "SELECT id, _rev, doc FROM docs", -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        i_set_error(error, error_size, sqlite3_errmsg((sqlite3 *)store->db));
        return -1;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        const char *rev = (const char *)sqlite3_column_text(stmt, 1);
        const char *doc = (const char *)sqlite3_column_text(stmt, 2);
        callback(id != NULL ? id : "", rev != NULL ? rev : "", doc != NULL ? doc : "", context);
    }
    sqlite3_finalize(stmt);
    return 0;
}

int nt_store_backup(const NtStore *store, const char *target, char *error, size_t error_size)
{
    sqlite3 *dest = NULL;
    sqlite3_backup *backup = NULL;
    int rc;

    if (store == NULL || store->db == NULL || target == NULL)
        return -1;
    remove(target);
    rc = sqlite3_open(target, &dest);
    if (rc != SQLITE_OK)
    {
        i_set_error(error, error_size, dest != NULL ? sqlite3_errmsg(dest) : "Cannot create backup");
        if (dest != NULL)
            sqlite3_close(dest);
        return -1;
    }
    backup = sqlite3_backup_init(dest, "main", (sqlite3 *)store->db, "main");
    if (backup == NULL)
    {
        i_set_error(error, error_size, sqlite3_errmsg(dest));
        sqlite3_close(dest);
        return -1;
    }
    rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    if (rc != SQLITE_DONE)
    {
        i_set_error(error, error_size, sqlite3_errmsg(dest));
        sqlite3_close(dest);
        return -1;
    }
    sqlite3_close(dest);
    return 0;
}

int nt_store_restore(NtStore *store, const char *source, char *error, size_t error_size)
{
    char path[NT_PATH_LEN];
    char tmp[NT_PATH_LEN];

    if (store == NULL || source == NULL || !nt_file_exists(source))
    {
        i_set_error(error, error_size, "Backup file not found");
        return -1;
    }
    snprintf(path, sizeof(path), "%s", store->path);
    snprintf(tmp, sizeof(tmp), "%s.restoring", path);
    remove(tmp);
    if (nt_file_copy(source, tmp) != 0)
    {
        i_set_error(error, error_size, "Cannot copy backup file");
        return -1;
    }
    nt_store_close(store);
    remove(path);
    if (rename(tmp, path) != 0)
    {
        i_set_error(error, error_size, "Cannot install restored database");
        return nt_store_open(path, store, error, error_size);
    }
    return nt_store_open(path, store, error, error_size);
}

int nt_store_vacuum(NtStore *store, char *error, size_t error_size)
{
    char *message = NULL;
    int rc;

    if (store == NULL || store->db == NULL)
        return -1;
    rc = sqlite3_exec((sqlite3 *)store->db, "VACUUM", NULL, NULL, &message);
    if (rc != SQLITE_OK)
    {
        i_set_error(error, error_size, message != NULL ? message : "VACUUM failed");
        sqlite3_free(message);
        return -1;
    }
    return 0;
}
