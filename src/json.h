#ifndef NAPTIMER_JSON_H
#define NAPTIMER_JSON_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    NT_JNULL,
    NT_JBOOL,
    NT_JNUM,
    NT_JSTR,
    NT_JARR,
    NT_JOBJ
} NtJType;

typedef struct NtJson NtJson;

struct NtJson
{
    NtJType type;
    union
    {
        int b;
        double number;
        char *string;
        struct
        {
            NtJson **items;
            size_t count;
            size_t capacity;
        } array;
        struct
        {
            char **keys;
            NtJson **values;
            size_t count;
            size_t capacity;
        } object;
    } u;
};

typedef struct
{
    char *data;
    size_t len;
    size_t capacity;
} NtJBuf;

NtJson *nt_json_parse(const char *text, char *error, size_t error_size);
void nt_json_free(NtJson *value);
NtJson *nt_json_get(const NtJson *object, const char *key);
const char *nt_json_str(const NtJson *value, const char *fallback);
int64_t nt_json_int(const NtJson *value, int64_t fallback);
int nt_json_bool(const NtJson *value, int fallback);
size_t nt_json_len(const NtJson *value);
NtJson *nt_json_at(const NtJson *value, size_t index);

void nt_jb_init(NtJBuf *buf);
void nt_jb_free(NtJBuf *buf);
void nt_jb_puts(NtJBuf *buf, const char *text);
void nt_jb_char(NtJBuf *buf, char ch);
void nt_jb_fmt(NtJBuf *buf, const char *format, ...);
void nt_jb_int(NtJBuf *buf, int64_t value);
void nt_jb_bool(NtJBuf *buf, int value);
void nt_jb_str(NtJBuf *buf, const char *text);
char *nt_jb_take(NtJBuf *buf);
void nt_json_quote(const char *text, char *out, size_t size);

#endif
