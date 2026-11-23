#include "report.h"

#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int task_id;
    int64_t seconds;
} NtReportRow;

void nt_report_range(NtReportType type, char from[NT_DATE_LEN], char to[NT_DATE_LEN])
{
    nt_today(to);
    switch (type)
    {
    case NT_REPORT_WEEKLY:
        nt_date_shift(to, -6, from);
        break;
    case NT_REPORT_MONTHLY:
        nt_date_month_shift(to, -1, from);
        break;
    case NT_REPORT_YEARLY:
        nt_date_year_shift(to, -1, from);
        break;
    case NT_REPORT_DAILY:
    default:
        snprintf(from, NT_DATE_LEN, "%s", to);
        break;
    }
}

static int i_row_cmp(const void *a, const void *b)
{
    const NtReportRow *ra = (const NtReportRow *)a;
    const NtReportRow *rb = (const NtReportRow *)b;
    if (ra->seconds < rb->seconds)
        return 1;
    if (ra->seconds > rb->seconds)
        return -1;
    return 0;
}

int nt_report_generate(const NtModel *model, NtReportType type, NtReportFormat format,
                       const char *from, const char *to, int rounding_minutes,
                       const char *tag_filter, char **out)
{
    NtReportRow *rows = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int64_t total = 0;
    NtJBuf buf;
    char start[NT_DATE_LEN];
    char end[NT_DATE_LEN];
    char duration[32];
    size_t i;
    int first = 1;

    (void)type;
    if (out != NULL)
        *out = NULL;
    snprintf(start, NT_DATE_LEN, "%s", from != NULL && from[0] != '\0' ? from : "1970-01-01");
    snprintf(end, NT_DATE_LEN, "%s", to != NULL && to[0] != '\0' ? to : "9999-12-31");
    if (nt_date_cmp(start, end) > 0)
    {
        char swap[NT_DATE_LEN];
        snprintf(swap, NT_DATE_LEN, "%s", start);
        snprintf(start, NT_DATE_LEN, "%s", end);
        snprintf(end, NT_DATE_LEN, "%s", swap);
    }

    for (i = 0; i < model->ntasks; i += 1)
    {
        const NtTask *task = &model->tasks[i];
        int64_t seconds = 0;
        size_t j;

        if (tag_filter != NULL && tag_filter[0] != '\0' &&
            !nt_model_task_has_tag(model, task->id, tag_filter))
            continue;
        for (j = 0; j < model->ndays; j += 1)
        {
            if (model->days[j].task_id == task->id &&
                nt_date_cmp(model->days[j].date, start) >= 0 &&
                nt_date_cmp(model->days[j].date, end) <= 0)
                seconds += model->days[j].seconds;
        }
        if (seconds == 0)
            continue;
        if (rounding_minutes > 0)
        {
            int64_t round = (int64_t)rounding_minutes * 60;
            seconds = ((seconds + round / 2) / round) * round;
        }
        if (count >= capacity)
        {
            size_t next = capacity == 0 ? 8 : capacity * 2;
            NtReportRow *grown = (NtReportRow *)realloc(rows, next * sizeof(NtReportRow));
            if (grown == NULL)
                break;
            rows = grown;
            capacity = next;
        }
        rows[count].task_id = task->id;
        rows[count].seconds = seconds;
        count += 1;
        total += seconds;
    }
    if (count > 1)
        qsort(rows, count, sizeof(NtReportRow), i_row_cmp);

    nt_jb_init(&buf);
    if (format == NT_REPORT_HTML)
    {
        nt_jb_puts(&buf, "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                         "<title>Naptimer Report</title><style>"
                         "body{font-family:sans-serif;margin:2rem;}h1{font-size:1.4rem;}"
                         "table{border-collapse:collapse;min-width:32rem;}"
                         "th,td{padding:.35rem .8rem;text-align:left;border-bottom:1px solid #ddd;}"
                         "td.dur{font-family:monospace;white-space:pre;}</style></head><body>");
        nt_jb_puts(&buf, "<h1>Naptimer Report</h1><h3>Period: ");
        nt_jb_puts(&buf, start);
        nt_jb_puts(&buf, " to ");
        nt_jb_puts(&buf, end);
        nt_jb_puts(&buf, "</h3><table><tr><th>Duration</th><th>Task</th><th>Project</th></tr>");
        for (i = 0; i < count; i += 1)
        {
            const NtTask *task = nt_model_task((NtModel *)model, rows[i].task_id);
            nt_format_duration(rows[i].seconds, duration, sizeof(duration));
            nt_jb_puts(&buf, "<tr><td class=\"dur\">");
            nt_jb_puts(&buf, duration);
            nt_jb_puts(&buf, "</td><td>");
            nt_jb_str(&buf, task != NULL ? task->name : "");
            nt_jb_puts(&buf, "</td><td>");
            nt_jb_str(&buf, task != NULL ? nt_model_project_name(model, task->project_id) : "");
            nt_jb_puts(&buf, "</td></tr>");
        }
        nt_format_duration(total, duration, sizeof(duration));
        nt_jb_puts(&buf, "<tr><th>");
        nt_jb_puts(&buf, duration);
        nt_jb_puts(&buf, "</th><th>Total</th><th></th></tr></table></body></html>");
    }
    else
    {
        nt_jb_puts(&buf, "Naptimer Report\nPeriod: ");
        nt_jb_puts(&buf, start);
        nt_jb_puts(&buf, " to ");
        nt_jb_puts(&buf, end);
        nt_jb_puts(&buf, "\n\n");
        if (count == 0)
        {
            nt_jb_puts(&buf, "No data found for this period.\n");
        }
        else
        {
            for (i = 0; i < count; i += 1)
            {
                const NtTask *task = nt_model_task((NtModel *)model, rows[i].task_id);
                const char *project;
                nt_format_duration(rows[i].seconds, duration, sizeof(duration));
                project = task != NULL ? nt_model_project_name(model, task->project_id) : "";
                nt_jb_puts(&buf, duration);
                nt_jb_puts(&buf, "  ");
                nt_jb_puts(&buf, task != NULL ? task->name : "");
                if (project[0] != '\0')
                {
                    nt_jb_puts(&buf, " (");
                    nt_jb_puts(&buf, project);
                    nt_jb_puts(&buf, ")");
                }
                nt_jb_puts(&buf, "\n");
            }
            nt_format_duration(total, duration, sizeof(duration));
            nt_jb_puts(&buf, "\n");
            nt_jb_puts(&buf, duration);
            nt_jb_puts(&buf, "  Total\n");
        }
    }
    (void)first;
    free(rows);
    if (out != NULL)
        *out = nt_jb_take(&buf);
    nt_jb_free(&buf);
    return 0;
}
