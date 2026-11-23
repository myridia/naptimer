#ifndef NAPTIMER_UTIL_H
#define NAPTIMER_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define NT_DATE_LEN 16
#define NT_PATH_LEN 1024

char *nt_strdup(const char *text);
void nt_str_trim(char *text);
int nt_contains_ci(const char *haystack, const char *needle);
int nt_index_ci(const char *haystack, const char *needle);

int64_t nt_now(void);
void nt_date_of(time_t value, int midnight_offset, char out[NT_DATE_LEN]);
void nt_today(char out[NT_DATE_LEN]);
int nt_date_valid(const char *date);
int nt_date_cmp(const char *a, const char *b);
void nt_date_shift(const char *date, int days, char out[NT_DATE_LEN]);
void nt_date_month_shift(const char *date, int months, char out[NT_DATE_LEN]);
void nt_date_year_shift(const char *date, int years, char out[NT_DATE_LEN]);
void nt_week_start(const char *date, int week_start_day, char out[NT_DATE_LEN]);
void nt_month_start(const char *date, char out[NT_DATE_LEN]);
void nt_year_start(const char *date, char out[NT_DATE_LEN]);
time_t nt_day_begin(const char *date, int midnight_offset);
time_t nt_day_after(const char *date, int midnight_offset);
void nt_format_duration(int64_t seconds, char *out, size_t size);
void nt_format_datetime(time_t value, char *out, size_t size);
void nt_format_date_long(const char *date, char *out, size_t size);

void nt_data_path(char *out, size_t size, const char *name);
void nt_config_path(char *out, size_t size, const char *name);
int nt_mkdir_p(const char *path);
int nt_file_exists(const char *path);
int nt_file_copy(const char *src, const char *dst);

#endif
