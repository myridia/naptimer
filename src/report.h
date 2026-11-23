#ifndef NAPTIMER_REPORT_H
#define NAPTIMER_REPORT_H

#include "model.h"

typedef enum
{
    NT_REPORT_DAILY,
    NT_REPORT_WEEKLY,
    NT_REPORT_MONTHLY,
    NT_REPORT_YEARLY
} NtReportType;

typedef enum
{
    NT_REPORT_TEXT,
    NT_REPORT_HTML
} NtReportFormat;

void nt_report_range(NtReportType type, char from[NT_DATE_LEN], char to[NT_DATE_LEN]);
int nt_report_generate(const NtModel *model, NtReportType type, NtReportFormat format,
                       const char *from, const char *to, int rounding_minutes,
                       const char *tag_filter, char **out);

#endif
