#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

char *nt_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL)
        return NULL;
    len = strlen(text) + 1;
    copy = (char *)malloc(len);
    if (copy != NULL)
        memcpy(copy, text, len);
    return copy;
}

void nt_str_trim(char *text)
{
    char *start;
    char *end;

    if (text == NULL)
        return;
    start = text;
    while (*start != '\0' && isspace((unsigned char)*start))
        start += 1;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1]))
        end -= 1;
    *end = '\0';
    if (start != text)
        memmove(text, start, (size_t)(end - start) + 1);
}

int nt_contains_ci(const char *haystack, const char *needle)
{
    return nt_index_ci(haystack, needle) >= 0;
}

int nt_index_ci(const char *haystack, const char *needle)
{
    size_t nlen;
    const char *p;

    if (haystack == NULL || needle == NULL)
        return -1;
    nlen = strlen(needle);
    if (nlen == 0)
        return 0;
    for (p = haystack; *p != '\0'; p += 1)
    {
        size_t i = 0;
        while (i < nlen && p[i] != '\0' &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            i += 1;
        if (i == nlen)
            return (int)(p - haystack);
    }
    return -1;
}

static void i_localtime(time_t value, struct tm *out)
{
#if defined(_WIN32)
    localtime_s(out, &value);
#else
    localtime_r(&value, out);
#endif
}

int64_t nt_now(void)
{
    return (int64_t)time(NULL);
}

void nt_date_of(time_t value, int midnight_offset, char out[NT_DATE_LEN])
{
    struct tm t;
    time_t shifted = value - (time_t)midnight_offset * 3600;

    i_localtime(shifted, &t);
    snprintf(out, NT_DATE_LEN, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

void nt_today(char out[NT_DATE_LEN])
{
    nt_date_of(time(NULL), 0, out);
}

int nt_date_valid(const char *date)
{
    int year = 0;
    int month = 0;
    int day = 0;

    if (date == NULL || strlen(date) != 10)
        return 0;
    if (sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3)
        return 0;
    return year >= 1970 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

int nt_date_cmp(const char *a, const char *b)
{
    if (a == NULL)
        a = "";
    if (b == NULL)
        b = "";
    return strcmp(a, b);
}

static int i_date_tm(const char *date, struct tm *out)
{
    int year, month, day;

    if (date == NULL || sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3)
        return 0;
    memset(out, 0, sizeof(*out));
    out->tm_year = year - 1900;
    out->tm_mon = month - 1;
    out->tm_mday = day;
    out->tm_hour = 12;
    out->tm_isdst = -1;
    return 1;
}

void nt_date_shift(const char *date, int days, char out[NT_DATE_LEN])
{
    struct tm t;
    time_t value;

    if (i_date_tm(date, &t) == 0)
    {
        snprintf(out, NT_DATE_LEN, "%s", date != NULL ? date : "");
        return;
    }
    value = mktime(&t) + (time_t)days * 86400;
    nt_date_of(value, 0, out);
}

void nt_date_month_shift(const char *date, int months, char out[NT_DATE_LEN])
{
    int year, month, day;

    if (date == NULL || sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3)
    {
        snprintf(out, NT_DATE_LEN, "%s", date != NULL ? date : "");
        return;
    }
    month -= 1 + months;
    year += month / 12;
    month %= 12;
    if (month < 0)
    {
        month += 12;
        year -= 1;
    }
    month += 1;
    if (day > 28)
    {
        char next[NT_DATE_LEN];
        struct tm probe;
        int last;

        snprintf(next, sizeof(next), "%04d-%02d-01", year + (month == 12 ? 1 : 0),
                 month == 12 ? 1 : month + 1);
        if (i_date_tm(next, &probe))
        {
            probe.tm_mday -= 1;
            mktime(&probe);
            last = probe.tm_mday;
            if (day > last)
                day = last;
        }
    }
    snprintf(out, NT_DATE_LEN, "%04d-%02d-%02d", year, month, day);
}

void nt_date_year_shift(const char *date, int years, char out[NT_DATE_LEN])
{
    int year, month, day;

    if (date == NULL || sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3)
    {
        snprintf(out, NT_DATE_LEN, "%s", date != NULL ? date : "");
        return;
    }
    snprintf(out, NT_DATE_LEN, "%04d-%02d-%02d", year + years, month, day);
}

void nt_week_start(const char *date, int week_start_day, char out[NT_DATE_LEN])
{
    struct tm t;
    time_t value;
    int iso;
    int delta;
    int base = week_start_day;

    if (i_date_tm(date, &t) == 0)
    {
        snprintf(out, NT_DATE_LEN, "%s", date != NULL ? date : "");
        return;
    }
    if (base < 1 || base > 7)
        base = 1;
    value = mktime(&t);
    i_localtime(value, &t);
    iso = t.tm_wday == 0 ? 7 : t.tm_wday;
    delta = (iso - base + 7) % 7;
    nt_date_of(value - (time_t)delta * 86400, 0, out);
}

void nt_month_start(const char *date, char out[NT_DATE_LEN])
{
    snprintf(out, NT_DATE_LEN, "%.7s-01", date != NULL ? date : "1970-01-01");
}

void nt_year_start(const char *date, char out[NT_DATE_LEN])
{
    snprintf(out, NT_DATE_LEN, "%.4s-01-01", date != NULL ? date : "1970");
}

time_t nt_day_begin(const char *date, int midnight_offset)
{
    struct tm t;
    time_t value;

    if (i_date_tm(date, &t) == 0)
        return 0;
    t.tm_hour = 0;
    value = mktime(&t);
    return value + (time_t)midnight_offset * 3600;
}

time_t nt_day_after(const char *date, int midnight_offset)
{
    char next[NT_DATE_LEN];
    nt_date_shift(date, 1, next);
    return nt_day_begin(next, midnight_offset);
}

void nt_format_duration(int64_t seconds, char *out, size_t size)
{
    int64_t hours;
    int minutes;
    int secs;

    if (seconds < 0)
        seconds = 0;
    hours = seconds / 3600;
    minutes = (int)((seconds % 3600) / 60);
    secs = (int)(seconds % 60);
    snprintf(out, size, "%02lld:%02d:%02d", (long long)hours, minutes, secs);
}

void nt_format_datetime(time_t value, char *out, size_t size)
{
    struct tm t;
    i_localtime(value, &t);
    strftime(out, size, "%Y-%m-%d %H:%M", &t);
}

void nt_format_date_long(const char *date, char *out, size_t size)
{
    struct tm t;
    time_t value;

    if (i_date_tm(date, &t) == 0)
    {
        snprintf(out, size, "%s", date != NULL ? date : "");
        return;
    }
    value = mktime(&t);
    i_localtime(value, &t);
    strftime(out, size, "%a, %b %d %Y", &t);
}

static int i_mkdir_one(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

int nt_mkdir_p(const char *path)
{
    char tmp[NT_PATH_LEN];
    char *p;
    size_t len;

    if (path == NULL || path[0] == '\0')
        return 0;
    len = strlen(path);
    if (len >= sizeof(tmp))
        return -1;
    memcpy(tmp, path, len + 1);
    for (p = tmp + 1; *p != '\0'; p += 1)
    {
        if (*p == '/' || *p == '\\')
        {
            *p = '\0';
            i_mkdir_one(tmp);
            *p = '/';
        }
    }
    return i_mkdir_one(tmp) == 0 ? 0 : -1;
}

static void i_base_path(char *out, size_t size, int config)
{
    const char *env = NULL;
    char base[NT_PATH_LEN];

#if defined(_WIN32)
    env = getenv("APPDATA");
    if (env == NULL || env[0] == '\0')
        env = getenv("USERPROFILE");
    if (env != NULL && env[0] != '\0')
        snprintf(base, sizeof(base), "%s\\Naptimer", env);
    else
        snprintf(base, sizeof(base), "Naptimer");
#else
    env = config ? getenv("XDG_CONFIG_HOME") : getenv("XDG_DATA_HOME");
    if (env == NULL || env[0] == '\0')
    {
        const char *home = getenv("HOME");
        if (home == NULL || home[0] == '\0')
            home = ".";
        snprintf(base, sizeof(base), "%s/%s", home, config ? ".config" : ".local/share");
    }
    else
    {
        snprintf(base, sizeof(base), "%s", env);
    }
    if (config)
        strncat(base, "/naptimer", sizeof(base) - strlen(base) - 1);
    else
        strncat(base, "/naptimer", sizeof(base) - strlen(base) - 1);
#endif
    snprintf(out, size, "%s", base);
    nt_mkdir_p(out);
}

void nt_data_path(char *out, size_t size, const char *name)
{
    char base[NT_PATH_LEN];
    i_base_path(base, sizeof(base), 0);
#if defined(_WIN32)
    snprintf(out, size, "%s\\%s", base, name);
#else
    snprintf(out, size, "%s/%s", base, name);
#endif
}

void nt_config_path(char *out, size_t size, const char *name)
{
    char base[NT_PATH_LEN];
    i_base_path(base, sizeof(base), 1);
#if defined(_WIN32)
    snprintf(out, size, "%s\\%s", base, name);
#else
    snprintf(out, size, "%s/%s", base, name);
#endif
}

int nt_file_exists(const char *path)
{
    FILE *file;
    if (path == NULL)
        return 0;
    file = fopen(path, "rb");
    if (file == NULL)
        return 0;
    fclose(file);
    return 1;
}

int nt_file_copy(const char *src, const char *dst)
{
    FILE *in = NULL;
    FILE *out = NULL;
    char buffer[65536];
    size_t n;
    int result = -1;

    if (src == NULL || dst == NULL)
        return -1;
    in = fopen(src, "rb");
    if (in == NULL)
        return -1;
    out = fopen(dst, "wb");
    if (out == NULL)
    {
        fclose(in);
        return -1;
    }
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        if (fwrite(buffer, 1, n, out) != n)
            goto done;
    }
    result = ferror(in) || ferror(out) ? -1 : 0;
done:
    fclose(in);
    fclose(out);
    return result;
}
