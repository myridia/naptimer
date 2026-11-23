#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define NT_JSON_MAX_DEPTH 64

typedef struct
{
    const char *p;
    char *error;
    size_t error_size;
    int failed;
} Parser;

static void i_error(Parser *parser, const char *message)
{
    if (parser->failed == 0)
    {
        parser->failed = 1;
        if (parser->error != NULL && parser->error_size > 0)
            snprintf(parser->error, parser->error_size, "%s", message);
    }
}

static void i_skip(Parser *parser)
{
    while (*parser->p != '\0' && isspace((unsigned char)*parser->p))
        parser->p += 1;
}

static NtJson *i_value(Parser *parser, int depth);

static void i_append_utf8(NtJBuf *buf, unsigned code)
{
    if (code < 0x80)
    {
        nt_jb_char(buf, (char)code);
    }
    else if (code < 0x800)
    {
        nt_jb_char(buf, (char)(0xC0 | (code >> 6)));
        nt_jb_char(buf, (char)(0x80 | (code & 0x3F)));
    }
    else if (code < 0x10000)
    {
        nt_jb_char(buf, (char)(0xE0 | (code >> 12)));
        nt_jb_char(buf, (char)(0x80 | ((code >> 6) & 0x3F)));
        nt_jb_char(buf, (char)(0x80 | (code & 0x3F)));
    }
    else
    {
        nt_jb_char(buf, (char)(0xF0 | (code >> 18)));
        nt_jb_char(buf, (char)(0x80 | ((code >> 12) & 0x3F)));
        nt_jb_char(buf, (char)(0x80 | ((code >> 6) & 0x3F)));
        nt_jb_char(buf, (char)(0x80 | (code & 0x3F)));
    }
}

static unsigned i_hex4(Parser *parser)
{
    unsigned value = 0;
    int i;
    for (i = 0; i < 4; i += 1)
    {
        char ch = *parser->p;
        unsigned digit;
        if (ch >= '0' && ch <= '9')
            digit = (unsigned)(ch - '0');
        else if (ch >= 'a' && ch <= 'f')
            digit = (unsigned)(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F')
            digit = (unsigned)(ch - 'A' + 10);
        else
        {
            i_error(parser, "Invalid Unicode escape in JSON");
            return 0;
        }
        value = value * 16 + digit;
        parser->p += 1;
    }
    return value;
}

static char *i_string(Parser *parser)
{
    NtJBuf buf;
    char *result;

    nt_jb_init(&buf);
    parser->p += 1;
    while (*parser->p != '\0' && parser->failed == 0)
    {
        char ch = *parser->p;
        if (ch == '"')
        {
            parser->p += 1;
            result = nt_jb_take(&buf);
            nt_jb_free(&buf);
            return result;
        }
        if (ch == '\\')
        {
            parser->p += 1;
            ch = *parser->p;
            switch (ch)
            {
            case '"': nt_jb_char(&buf, '"'); parser->p += 1; break;
            case '\\': nt_jb_char(&buf, '\\'); parser->p += 1; break;
            case '/': nt_jb_char(&buf, '/'); parser->p += 1; break;
            case 'b': nt_jb_char(&buf, '\b'); parser->p += 1; break;
            case 'f': nt_jb_char(&buf, '\f'); parser->p += 1; break;
            case 'n': nt_jb_char(&buf, '\n'); parser->p += 1; break;
            case 'r': nt_jb_char(&buf, '\r'); parser->p += 1; break;
            case 't': nt_jb_char(&buf, '\t'); parser->p += 1; break;
            case 'u':
            {
                unsigned code;
                parser->p += 1;
                code = i_hex4(parser);
                if (code >= 0xD800 && code <= 0xDBFF && parser->p[0] == '\\' && parser->p[1] == 'u')
                {
                    unsigned low;
                    parser->p += 2;
                    low = i_hex4(parser);
                    if (low >= 0xDC00 && low <= 0xDFFF)
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                i_append_utf8(&buf, code);
                break;
            }
            default:
                i_error(parser, "Invalid escape in JSON string");
                break;
            }
            continue;
        }
        if ((unsigned char)ch < 0x20)
        {
            i_error(parser, "Control character in JSON string");
            break;
        }
        nt_jb_char(&buf, ch);
        parser->p += 1;
    }
    i_error(parser, "Unterminated JSON string");
    nt_jb_free(&buf);
    return NULL;
}

static NtJson *i_new(NtJType type)
{
    NtJson *value = (NtJson *)calloc(1, sizeof(NtJson));
    if (value != NULL)
        value->type = type;
    return value;
}

static NtJson *i_number(Parser *parser)
{
    char tmp[64];
    size_t n = 0;
    NtJson *value;

    while (*parser->p != '\0' && n + 1 < sizeof(tmp) &&
           (isdigit((unsigned char)*parser->p) || strchr("+-.eE", *parser->p) != NULL))
    {
        tmp[n++] = *parser->p;
        parser->p += 1;
    }
    tmp[n] = '\0';
    if (n == 0)
    {
        i_error(parser, "Invalid JSON number");
        return NULL;
    }
    value = i_new(NT_JNUM);
    if (value != NULL)
        value->u.number = strtod(tmp, NULL);
    return value;
}

static NtJson *i_array(Parser *parser, int depth)
{
    NtJson *value = i_new(NT_JARR);
    parser->p += 1;
    i_skip(parser);
    if (*parser->p == ']')
    {
        parser->p += 1;
        return value;
    }
    while (parser->failed == 0)
    {
        NtJson *item = i_value(parser, depth + 1);
        NtJson **items;
        if (parser->failed)
        {
            nt_json_free(item);
            break;
        }
        items = (NtJson **)realloc(value->u.array.items,
                                   (value->u.array.count + 1) * sizeof(NtJson *));
        if (items == NULL)
        {
            i_error(parser, "Out of memory parsing JSON");
            nt_json_free(item);
            break;
        }
        value->u.array.items = items;
        value->u.array.items[value->u.array.count++] = item;
        i_skip(parser);
        if (*parser->p == ',')
        {
            parser->p += 1;
            i_skip(parser);
            continue;
        }
        if (*parser->p == ']')
        {
            parser->p += 1;
            break;
        }
        i_error(parser, "Expected ',' or ']' in JSON array");
    }
    return value;
}

static NtJson *i_object(Parser *parser, int depth)
{
    NtJson *value = i_new(NT_JOBJ);
    parser->p += 1;
    i_skip(parser);
    if (*parser->p == '}')
    {
        parser->p += 1;
        return value;
    }
    while (parser->failed == 0)
    {
        char *key;
        NtJson *item;
        char **keys;
        NtJson **values;

        if (*parser->p != '"')
        {
            i_error(parser, "Expected object key in JSON");
            break;
        }
        key = i_string(parser);
        if (parser->failed)
        {
            free(key);
            break;
        }
        i_skip(parser);
        if (*parser->p != ':')
        {
            i_error(parser, "Expected ':' in JSON object");
            free(key);
            break;
        }
        parser->p += 1;
        i_skip(parser);
        item = i_value(parser, depth + 1);
        if (parser->failed)
        {
            free(key);
            nt_json_free(item);
            break;
        }
        keys = (char **)realloc(value->u.object.keys,
                                (value->u.object.count + 1) * sizeof(char *));
        values = (NtJson **)realloc(value->u.object.values,
                                    (value->u.object.count + 1) * sizeof(NtJson *));
        if (keys == NULL || values == NULL)
        {
            i_error(parser, "Out of memory parsing JSON");
            free(key);
            nt_json_free(item);
            break;
        }
        value->u.object.keys = keys;
        value->u.object.values = values;
        value->u.object.keys[value->u.object.count] = key;
        value->u.object.values[value->u.object.count] = item;
        value->u.object.count += 1;
        i_skip(parser);
        if (*parser->p == ',')
        {
            parser->p += 1;
            i_skip(parser);
            continue;
        }
        if (*parser->p == '}')
        {
            parser->p += 1;
            break;
        }
        i_error(parser, "Expected ',' or '}' in JSON object");
    }
    return value;
}

static NtJson *i_value(Parser *parser, int depth)
{
    NtJson *value = NULL;

    i_skip(parser);
    if (depth > NT_JSON_MAX_DEPTH)
    {
        i_error(parser, "JSON nesting too deep");
        return NULL;
    }
    switch (*parser->p)
    {
    case '{':
        return i_object(parser, depth);
    case '[':
        return i_array(parser, depth);
    case '"':
    {
        char *text = i_string(parser);
        value = i_new(NT_JSTR);
        if (value != NULL && text != NULL)
            value->u.string = text;
        else
            free(text);
        return value;
    }
    case 't':
        if (strncmp(parser->p, "true", 4) == 0)
        {
            parser->p += 4;
            value = i_new(NT_JBOOL);
            if (value != NULL)
                value->u.b = 1;
            return value;
        }
        break;
    case 'f':
        if (strncmp(parser->p, "false", 5) == 0)
        {
            parser->p += 5;
            value = i_new(NT_JBOOL);
            return value;
        }
        break;
    case 'n':
        if (strncmp(parser->p, "null", 4) == 0)
        {
            parser->p += 4;
            return i_new(NT_JNULL);
        }
        break;
    default:
        return i_number(parser);
    }
    i_error(parser, "Invalid JSON value");
    return NULL;
}

NtJson *nt_json_parse(const char *text, char *error, size_t error_size)
{
    Parser parser;
    NtJson *value;

    if (error != NULL && error_size > 0)
        error[0] = '\0';
    parser.p = text != NULL ? text : "";
    parser.error = error;
    parser.error_size = error_size;
    parser.failed = 0;
    value = i_value(&parser, 0);
    if (parser.failed)
    {
        nt_json_free(value);
        return NULL;
    }
    i_skip(&parser);
    if (*parser.p != '\0')
    {
        nt_json_free(value);
        if (error != NULL && error_size > 0 && error[0] == '\0')
            snprintf(error, error_size, "Trailing characters after JSON value");
        return NULL;
    }
    return value;
}

void nt_json_free(NtJson *value)
{
    size_t i;

    if (value == NULL)
        return;
    switch (value->type)
    {
    case NT_JSTR:
        free(value->u.string);
        break;
    case NT_JARR:
        for (i = 0; i < value->u.array.count; i += 1)
            nt_json_free(value->u.array.items[i]);
        free(value->u.array.items);
        break;
    case NT_JOBJ:
        for (i = 0; i < value->u.object.count; i += 1)
        {
            free(value->u.object.keys[i]);
            nt_json_free(value->u.object.values[i]);
        }
        free(value->u.object.keys);
        free(value->u.object.values);
        break;
    default:
        break;
    }
    free(value);
}

NtJson *nt_json_get(const NtJson *object, const char *key)
{
    size_t i;
    if (object == NULL || object->type != NT_JOBJ || key == NULL)
        return NULL;
    for (i = 0; i < object->u.object.count; i += 1)
    {
        if (strcmp(object->u.object.keys[i], key) == 0)
            return object->u.object.values[i];
    }
    return NULL;
}

const char *nt_json_str(const NtJson *value, const char *fallback)
{
    if (value != NULL && value->type == NT_JSTR && value->u.string != NULL)
        return value->u.string;
    return fallback;
}

int64_t nt_json_int(const NtJson *value, int64_t fallback)
{
    if (value == NULL)
        return fallback;
    if (value->type == NT_JNUM)
        return (int64_t)value->u.number;
    if (value->type == NT_JBOOL)
        return value->u.b ? 1 : 0;
    return fallback;
}

int nt_json_bool(const NtJson *value, int fallback)
{
    if (value == NULL)
        return fallback;
    if (value->type == NT_JBOOL)
        return value->u.b;
    if (value->type == NT_JNUM)
        return value->u.number != 0;
    return fallback;
}

size_t nt_json_len(const NtJson *value)
{
    if (value == NULL)
        return 0;
    if (value->type == NT_JARR)
        return value->u.array.count;
    if (value->type == NT_JOBJ)
        return value->u.object.count;
    return 0;
}

NtJson *nt_json_at(const NtJson *value, size_t index)
{
    if (value == NULL || value->type != NT_JARR || index >= value->u.array.count)
        return NULL;
    return value->u.array.items[index];
}

static void i_grow(NtJBuf *buf, size_t extra)
{
    size_t needed;
    char *data;

    if (buf->capacity - buf->len > extra)
        return;
    needed = buf->len + extra + 1;
    if (needed < buf->capacity * 2)
        needed = buf->capacity * 2;
    data = (char *)realloc(buf->data, needed);
    if (data != NULL)
    {
        buf->data = data;
        buf->capacity = needed;
    }
}

void nt_jb_init(NtJBuf *buf)
{
    buf->data = NULL;
    buf->len = 0;
    buf->capacity = 0;
}

void nt_jb_free(NtJBuf *buf)
{
    free(buf->data);
    nt_jb_init(buf);
}

void nt_jb_puts(NtJBuf *buf, const char *text)
{
    size_t len;
    if (text == NULL)
        return;
    len = strlen(text);
    i_grow(buf, len);
    memcpy(buf->data + buf->len, text, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
}

void nt_jb_char(NtJBuf *buf, char ch)
{
    i_grow(buf, 1);
    buf->data[buf->len++] = ch;
    buf->data[buf->len] = '\0';
}

void nt_jb_fmt(NtJBuf *buf, const char *format, ...)
{
    va_list args;
    va_list copy;
    int size;
    char *tmp;

    va_start(args, format);
    va_copy(copy, args);
    size = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (size > 0)
    {
        tmp = (char *)malloc((size_t)size + 1);
        if (tmp != NULL)
        {
            vsnprintf(tmp, (size_t)size + 1, format, args);
            nt_jb_puts(buf, tmp);
            free(tmp);
        }
    }
    va_end(args);
}

void nt_jb_int(NtJBuf *buf, int64_t value)
{
    nt_jb_fmt(buf, "%lld", (long long)value);
}

void nt_jb_bool(NtJBuf *buf, int value)
{
    nt_jb_puts(buf, value ? "true" : "false");
}

void nt_jb_str(NtJBuf *buf, const char *text)
{
    const unsigned char *p;

    nt_jb_char(buf, '"');
    if (text != NULL)
    {
        for (p = (const unsigned char *)text; *p != '\0'; p += 1)
        {
            switch (*p)
            {
            case '"': nt_jb_puts(buf, "\\\""); break;
            case '\\': nt_jb_puts(buf, "\\\\"); break;
            case '\b': nt_jb_puts(buf, "\\b"); break;
            case '\f': nt_jb_puts(buf, "\\f"); break;
            case '\n': nt_jb_puts(buf, "\\n"); break;
            case '\r': nt_jb_puts(buf, "\\r"); break;
            case '\t': nt_jb_puts(buf, "\\t"); break;
            default:
                if (*p < 0x20)
                    nt_jb_fmt(buf, "\\u%04x", (unsigned)*p);
                else
                    nt_jb_char(buf, (char)*p);
                break;
            }
        }
    }
    nt_jb_char(buf, '"');
}

char *nt_jb_take(NtJBuf *buf)
{
    char *result;

    if (buf->data == NULL)
    {
        result = (char *)malloc(1);
        if (result != NULL)
            result[0] = '\0';
        return result;
    }
    result = buf->data;
    buf->data = NULL;
    buf->len = 0;
    buf->capacity = 0;
    return result;
}

void nt_json_quote(const char *text, char *out, size_t size)
{
    NtJBuf buf;
    char *quoted;

    nt_jb_init(&buf);
    nt_jb_str(&buf, text);
    quoted = nt_jb_take(&buf);
    nt_jb_free(&buf);
    snprintf(out, size, "%s", quoted != NULL ? quoted : "\"\"");
    free(quoted);
}
