#include "cli.h"

#include "json.h"
#include "report.h"
#include "timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NAPTIMER_VERSION
#define NAPTIMER_VERSION "0.1.0"
#endif

typedef struct
{
    int quiet;
    int json;
    int show_version;
    int show_status;
    int list_tasks;
    int show_today;
    int show_week;
    int show_month;
    int stop_timer;
    int stop_all;
    int total_time;
    int active_time;
    int summary;
    int list_projects;
    int list_tags;
    int vacuum;
    int task_id;
    int delete_task;
    int hide_task;
    int unhide_task;
    int reset_task;
    int duplicate_task;
    int move_task;
    int move_to_project;
    int task_details;
    int delete_project;
    int rename_project;
    int merge_source;
    int merge_target;
    int list_annotations;
    int annotate_task;
    int add_tag_task;
    int remove_tag_task;
    const char *add_task;
    const char *project;
    const char *report;
    const char *report_file;
    const char *export_csv;
    const char *export_json;
    const char *import_csv;
    const char *database;
    const char *datadir;
    const char *since;
    const char *until;
    const char *date;
    const char *rename_task;
    const char *rename_project_to;
    const char *annotate;
    const char *note;
    const char *add_tag;
    const char *remove_tag;
    const char *tag;
    const char *backup;
    const char *restore;
    const char *export_sqlite;
    const char *add_project;
} Opts;

static int i_is(const char *arg, const char *short_name, const char *long_name)
{
    if (short_name != NULL && strcmp(arg, short_name) == 0)
        return 1;
    if (long_name != NULL && strcmp(arg, long_name) == 0)
        return 1;
    return 0;
}

static const char *i_next(int argc, char *argv[], int *i)
{
    if (*i + 1 >= argc)
        return NULL;
    *i += 1;
    return argv[*i];
}

static int i_next_int(int argc, char *argv[], int *i, int fallback)
{
    const char *value = i_next(argc, argv, i);
    return value != NULL ? atoi(value) : fallback;
}

static void i_parse(int argc, char *argv[], Opts *opts)
{
    int i;
    memset(opts, 0, sizeof(*opts));
    opts->project = NULL;
    opts->task_id = 0;
    opts->merge_source = 0;
    opts->merge_target = 0;

    for (i = 1; i < argc; i += 1)
    {
        const char *arg = argv[i];
        if (i_is(arg, "-s", "--start")) opts->task_id = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, "-t", "--stop")) opts->stop_timer = 1;
        else if (i_is(arg, "-S", "--stop-all")) opts->stop_all = 1;
        else if (i_is(arg, "-u", "--status")) opts->show_status = 1;
        else if (i_is(arg, "-l", "--list-tasks")) opts->list_tasks = 1;
        else if (i_is(arg, "-a", "--add-task")) opts->add_task = i_next(argc, argv, &i);
        else if (i_is(arg, "-p", "--project")) opts->project = i_next(argc, argv, &i);
        else if (i_is(arg, "-r", "--report")) opts->report = i_next(argc, argv, &i);
        else if (i_is(arg, "-f", "--report-file")) opts->report_file = i_next(argc, argv, &i);
        else if (i_is(arg, "-e", "--export-csv")) opts->export_csv = i_next(argc, argv, &i);
        else if (i_is(arg, "-i", "--import-csv")) opts->import_csv = i_next(argc, argv, &i);
        else if (i_is(arg, "-v", "--version")) opts->show_version = 1;
        else if (i_is(arg, "-d", "--datadir")) opts->datadir = i_next(argc, argv, &i);
        else if (i_is(arg, "-D", "--database")) opts->database = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--verbose")) { }
        else if (i_is(arg, "-q", "--quiet")) opts->quiet = 1;
        else if (i_is(arg, NULL, "--today")) opts->show_today = 1;
        else if (i_is(arg, NULL, "--week")) opts->show_week = 1;
        else if (i_is(arg, NULL, "--month")) opts->show_month = 1;
        else if (i_is(arg, NULL, "--since")) opts->since = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--until")) opts->until = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--date")) opts->date = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--delete-task")) opts->delete_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--hide-task")) opts->hide_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--unhide-task")) opts->unhide_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--rename-task")) opts->rename_task = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--rename-task-id")) opts->task_id = i_next_int(argc, argv, &i, opts->task_id);
        else if (i_is(arg, NULL, "--move-task")) opts->move_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--move-to-project")) opts->move_to_project = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--task-details")) opts->task_details = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--reset-task")) opts->reset_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--duplicate")) opts->duplicate_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--list-projects")) opts->list_projects = 1;
        else if (i_is(arg, NULL, "--add-project")) opts->add_project = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--delete-project")) opts->delete_project = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--rename-project")) opts->rename_project = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--rename-project-to")) opts->rename_project_to = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--annotate")) opts->annotate = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--annotate-task")) opts->annotate_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--list-annotations")) opts->list_annotations = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--note")) opts->note = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--add-tag")) opts->add_tag = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--add-tag-task")) opts->add_tag_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--remove-tag")) opts->remove_tag = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--remove-tag-task")) opts->remove_tag_task = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--list-tags")) opts->list_tags = 1;
        else if (i_is(arg, NULL, "--tag")) opts->tag = i_next(argc, argv, &i);
        else if (i_is(arg, "-j", "--json")) opts->json = 1;
        else if (i_is(arg, "-J", "--export-json")) opts->export_json = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--backup")) opts->backup = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--restore")) opts->restore = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--export-sqlite")) opts->export_sqlite = i_next(argc, argv, &i);
        else if (i_is(arg, NULL, "--total-time")) opts->total_time = 1;
        else if (i_is(arg, NULL, "--active-time")) opts->active_time = 1;
        else if (i_is(arg, NULL, "--summary")) opts->summary = 1;
        else if (i_is(arg, NULL, "--merge-source")) opts->merge_source = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--merge-target")) opts->merge_target = i_next_int(argc, argv, &i, 0);
        else if (i_is(arg, NULL, "--vacuum")) opts->vacuum = 1;
        else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) opts->show_version = 1; /* handled as CLI trigger; gtimer-like */
    }
}

static int i_any_cli(const Opts *opts)
{
    return opts->show_version || opts->show_status || opts->list_tasks ||
           opts->stop_timer || opts->stop_all || opts->task_id > 0 ||
           opts->add_task != NULL || opts->report != NULL || opts->export_csv != NULL ||
           opts->export_json != NULL || opts->import_csv != NULL ||
           opts->show_today || opts->show_week || opts->show_month ||
           opts->since != NULL || opts->until != NULL || opts->date != NULL ||
           opts->delete_task > 0 || opts->hide_task > 0 || opts->unhide_task > 0 ||
           opts->rename_task != NULL || opts->move_task > 0 || opts->task_details > 0 ||
           opts->reset_task > 0 || opts->duplicate_task > 0 || opts->list_projects ||
           opts->add_project != NULL || opts->delete_project > 0 || opts->rename_project > 0 || opts->annotate != NULL ||
           opts->list_annotations > 0 || opts->note != NULL || opts->add_tag != NULL ||
           opts->remove_tag != NULL || opts->list_tags || opts->backup != NULL ||
           opts->restore != NULL || opts->export_sqlite != NULL || opts->total_time ||
           opts->active_time || opts->summary || opts->merge_source > 0 || opts->vacuum;
}

static void i_json_escape(const char *text)
{
    const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");
    putchar('"');
    while (*p != '\0')
    {
        switch (*p)
        {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20)
                printf("\\u%04x", (unsigned)*p);
            else
                putchar(*p);
            break;
        }
        p += 1;
    }
    putchar('"');
}

static void i_print_status(NtModel *model, int json)
{
    size_t i;
    int first = 1;

    if (json)
    {
        puts("[");
        for (i = 0; i < model->ntasks; i += 1)
        {
            NtTask *task = &model->tasks[i];
            if (!task->timing)
                continue;
            if (!first)
                puts(",");
            first = 0;
            printf("  {\"id\": %d, \"name\": ", task->id);
            i_json_escape(task->name);
            printf(", \"project\": ");
            i_json_escape(nt_model_project_name(model, task->project_id));
            printf("}");
        }
        puts(first ? "]" : "\n]");
        return;
    }

    for (i = 0; i < model->ntasks; i += 1)
    {
        NtTask *task = &model->tasks[i];
        const char *project;
        if (!task->timing)
            continue;
        first = 0;
        project = nt_model_project_name(model, task->project_id);
        if (project[0] != '\0')
            printf("[%d] %s (%s)\n", task->id, task->name, project);
        else
            printf("[%d] %s\n", task->id, task->name);
    }
    if (first)
        puts("No tasks are currently timing.");
}

static void i_list_tasks(NtModel *model, const Opts *opts)
{
    size_t i;
    int count = 0;

    if (opts->json)
    {
        int first = 1;
        puts("[");
        for (i = 0; i < model->ntasks; i += 1)
        {
            NtTask *task = &model->tasks[i];
            if (opts->tag != NULL && opts->tag[0] != '\0' && !nt_model_task_has_tag(model, task->id, opts->tag))
                continue;
            if (!first)
                puts(",");
            first = 0;
            count += 1;
            printf("  {\"id\": %d, \"name\": ", task->id);
            i_json_escape(task->name);
            printf(", \"project\": ");
            i_json_escape(nt_model_project_name(model, task->project_id));
            printf(", \"tags\": ");
            i_json_escape(task->tags);
            printf(", \"total_time\": %lld, \"today_time\": %lld}",
                   (long long)nt_model_total_seconds(model, task->id),
                   (long long)nt_model_day_seconds(model, task->id, NULL));
        }
        puts(first ? "]" : "\n]");
        return;
    }

    puts("ID   Project                     Task Name");
    puts("---- --------------------------- --------------------------------");
    for (i = 0; i < model->ntasks; i += 1)
    {
        NtTask *task = &model->tasks[i];
        const char *project = nt_model_project_name(model, task->project_id);
        if (opts->tag != NULL && opts->tag[0] != '\0' && !nt_model_task_has_tag(model, task->id, opts->tag))
            continue;
        count += 1;
        if (task->tags[0] != '\0')
            printf("%-4d %-27s %s [%s]\n", task->id, project[0] != '\0' ? project : "-", task->name, task->tags);
        else
            printf("%-4d %-27s %s\n", task->id, project[0] != '\0' ? project : "-", task->name);
    }
    printf("\n%d tasks total.\n", count);
}

static int i_handle_timer(NtModel *model, const Opts *opts)
{
    if (opts->stop_all)
    {
        size_t i;
        int count = 0;
        for (i = 0; i < model->ntasks; i += 1)
        {
            if (model->tasks[i].timing)
            {
                printf("Stopped: %s\n", model->tasks[i].name);
                nt_timer_stop(model, model->tasks[i].id);
                count += 1;
            }
        }
        printf("Stopped %d task(s).\n", count);
        return 1;
    }
    if (opts->stop_timer)
    {
        size_t i;
        int count = 0;
        for (i = 0; i < model->ntasks; i += 1)
        {
            if (model->tasks[i].timing)
            {
                printf("Stopped: %s\n", model->tasks[i].name);
                nt_timer_stop(model, model->tasks[i].id);
                count += 1;
            }
        }
        if (count == 0)
            puts("No task is currently timing.");
        return 1;
    }
    if (opts->task_id > 0 && opts->rename_task == NULL && opts->annotate_task <= 0 &&
        opts->move_task <= 0 && opts->task_details <= 0 && opts->reset_task <= 0 &&
        opts->duplicate_task <= 0)
    {
        NtTask *task = nt_model_task(model, opts->task_id);
        if (task == NULL)
        {
            fprintf(stderr, "Error: Task with ID %d not found\n", opts->task_id);
            return 1;
        }
        nt_timer_start(model, task->id, 0);
        printf("Started timing: %s (ID: %d)\n", task->name, task->id);
        return 1;
    }
    return 0;
}

static int i_handle_task_management(NtModel *model, const Opts *opts)
{
    if (opts->add_task != NULL)
    {
        int project_id = -1;
        NtTask *task;
        if (opts->project != NULL)
            project_id = atoi(opts->project);
        task = nt_model_add_task(model, opts->add_task, project_id, "");
        if (task != NULL)
            printf("Created task: %s\n", task->name);
        else
            fprintf(stderr, "Error creating task\n");
        return 1;
    }
    if (opts->delete_task > 0)
    {
        if (nt_model_delete_task(model, opts->delete_task) == 0)
            printf("Deleted task ID %d\n", opts->delete_task);
        else
            fprintf(stderr, "Error deleting task: not found\n");
        return 1;
    }
    if (opts->hide_task > 0)
    {
        if (nt_model_hide_task(model, opts->hide_task, 1) == 0)
            printf("Hidden task ID %d\n", opts->hide_task);
        else
            fprintf(stderr, "Error hiding task\n");
        return 1;
    }
    if (opts->unhide_task > 0)
    {
        if (nt_model_hide_task(model, opts->unhide_task, 0) == 0)
            printf("Unhidden task ID %d\n", opts->unhide_task);
        else
            fprintf(stderr, "Error unhiding task\n");
        return 1;
    }
    if (opts->rename_task != NULL && opts->task_id > 0)
    {
        NtTask *task = nt_model_task(model, opts->task_id);
        if (task == NULL)
        {
            fprintf(stderr, "Task ID %d not found\n", opts->task_id);
            return 1;
        }
        nt_model_update_task(model, opts->task_id, opts->rename_task, task->project_id, NULL);
        printf("Renamed task %d to: %s\n", opts->task_id, opts->rename_task);
        return 1;
    }
    if (opts->move_task > 0 && opts->move_to_project > 0)
    {
        NtTask *task = nt_model_task(model, opts->move_task);
        if (task == NULL)
        {
            fprintf(stderr, "Task ID %d not found\n", opts->move_task);
            return 1;
        }
        nt_model_update_task(model, opts->move_task, task->name, opts->move_to_project, NULL);
        printf("Moved task %d to project %d\n", opts->move_task, opts->move_to_project);
        return 1;
    }
    if (opts->task_details > 0)
    {
        NtTask *task = nt_model_task(model, opts->task_details);
        if (task == NULL)
        {
            fprintf(stderr, "Task ID %d not found\n", opts->task_details);
            return 1;
        }
        if (opts->json)
        {
            printf("{\n  \"id\": %d,\n  \"name\": ", task->id); i_json_escape(task->name);
            printf(",\n  \"project\": "); i_json_escape(nt_model_project_name(model, task->project_id));
            printf(",\n  \"tags\": "); i_json_escape(task->tags);
            printf(",\n  \"total_time\": %lld,\n  \"today_time\": %lld,\n  \"is_timing\": %s,\n  \"is_hidden\": %s\n}\n",
                   (long long)nt_model_total_seconds(model, task->id),
                   (long long)nt_model_day_seconds(model, task->id, NULL),
                   task->timing ? "true" : "false", task->hidden ? "true" : "false");
        }
        else
        {
            puts("Task Details:");
            printf("  ID: %d\n", task->id);
            printf("  Name: %s\n", task->name);
            printf("  Project: %s\n", task->project_id >= 0 ? nt_model_project_name(model, task->project_id) : "(none)");
            printf("  Tags: %s\n", task->tags[0] != '\0' ? task->tags : "(none)");
            printf("  Total Time: %lld seconds\n", (long long)nt_model_total_seconds(model, task->id));
            printf("  Today: %lld seconds\n", (long long)nt_model_day_seconds(model, task->id, NULL));
            printf("  Status: %s\n", task->timing ? "timing" : "stopped");
        }
        return 1;
    }
    if (opts->reset_task > 0)
    {
        if (nt_model_task(model, opts->reset_task) == NULL)
            fprintf(stderr, "Task ID %d not found\n", opts->reset_task);
        else
        {
            nt_model_reset_time(model, opts->reset_task);
            printf("Reset time for task %d\n", opts->reset_task);
        }
        return 1;
    }
    if (opts->duplicate_task > 0)
    {
        NtTask *task = nt_model_task(model, opts->duplicate_task);
        if (task == NULL)
            fprintf(stderr, "Task ID %d not found\n", opts->duplicate_task);
        else
        {
            NtTask *copy = nt_model_add_task(model, task->name, task->project_id, task->tags);
            if (copy != NULL)
                printf("Duplicated task %d as %d\n", task->id, copy->id);
        }
        return 1;
    }
    return 0;
}

static NtReportType i_report_type(const char *text)
{
    if (text == NULL)
        return NT_REPORT_DAILY;
    if (strcmp(text, "weekly") == 0 || strcmp(text, "w") == 0)
        return NT_REPORT_WEEKLY;
    if (strcmp(text, "monthly") == 0 || strcmp(text, "m") == 0)
        return NT_REPORT_MONTHLY;
    if (strcmp(text, "yearly") == 0 || strcmp(text, "y") == 0)
        return NT_REPORT_YEARLY;
    return NT_REPORT_DAILY;
}

static int i_handle_reports(NtModel *model, const Opts *opts)
{
    char from[NT_DATE_LEN];
    char to[NT_DATE_LEN];
    char *report = NULL;
    NtReportType type = NT_REPORT_DAILY;

    if (opts->report != NULL)
    {
        if (strcmp(opts->report, "daily") != 0 && strcmp(opts->report, "d") != 0 &&
            strcmp(opts->report, "weekly") != 0 && strcmp(opts->report, "w") != 0 &&
            strcmp(opts->report, "monthly") != 0 && strcmp(opts->report, "m") != 0 &&
            strcmp(opts->report, "yearly") != 0 && strcmp(opts->report, "y") != 0)
        {
            fprintf(stderr, "Error: Unknown report type '%s'. Use daily, weekly, or monthly\n", opts->report);
            return 1;
        }
        type = i_report_type(opts->report);
        nt_today(to);
        nt_report_range(type, from, to);
    }
    else if (opts->date != NULL)
    {
        snprintf(from, sizeof(from), "%s", opts->date);
        snprintf(to, sizeof(to), "%s", opts->date);
        printf("Report for %s:\n", opts->date);
    }
    else if (opts->since != NULL || opts->until != NULL)
    {
        snprintf(from, sizeof(from), "%s", opts->since != NULL ? opts->since : "1970-01-01");
        snprintf(to, sizeof(to), "%s", opts->until != NULL ? opts->until : "");
        if (to[0] == '\0')
            nt_today(to);
        printf("Report from %s to %s:\n", opts->since != NULL ? opts->since : "beginning",
               opts->until != NULL ? opts->until : "now");
    }
    else if (opts->show_week)
    {
        nt_today(to);
        nt_date_shift(to, -6, from);
        puts("Weekly Report:");
    }
    else if (opts->show_month)
    {
        nt_today(to);
        nt_date_month_shift(to, -1, from);
        puts("Monthly Report:");
    }
    else if (opts->show_today)
    {
        nt_today(to);
        snprintf(from, sizeof(from), "%s", to);
        puts("Today's Summary:");
    }
    else
    {
        return 0;
    }

    nt_report_generate(model, type, NT_REPORT_TEXT, from, to, 0, NULL, &report);
    if (opts->report != NULL && opts->report_file != NULL)
    {
        FILE *file = fopen(opts->report_file, "wb");
        if (file == NULL)
            fprintf(stderr, "Error: Failed to write report to %s\n", opts->report_file);
        else
        {
            fputs(report, file);
            fclose(file);
            printf("Report saved to: %s\n", opts->report_file);
        }
    }
    else
    {
        fputs(report, stdout);
    }
    free(report);
    return 1;
}

static int i_handle_projects(NtModel *model, const Opts *opts)
{
    if (opts->add_project != NULL)
    {
        NtProject *project = nt_model_add_project(model, opts->add_project);
        if (project != NULL)
            printf("Created project: %s\n", project->name);
        else
            fprintf(stderr, "Error creating project\n");
        return 1;
    }
    if (opts->list_projects)
    {
        size_t i;
        if (opts->json)
        {
            puts("[");
            for (i = 0; i < model->nprojects; i += 1)
            {
                printf("  {\"id\": %d, \"name\": ", model->projects[i].id);
                i_json_escape(model->projects[i].name);
                printf("}%s\n", i + 1 < model->nprojects ? "," : "");
            }
            puts("]");
        }
        else
        {
            puts("ID   Project Name");
            puts("---- ---------------------------");
            for (i = 0; i < model->nprojects; i += 1)
                printf("%-4d %s\n", model->projects[i].id, model->projects[i].name);
            printf("\n%zu projects total.\n", model->nprojects);
        }
        return 1;
    }
    if (opts->delete_project > 0)
    {
        fprintf(stderr, "Delete project not yet implemented (requires db function)\n");
        return 1;
    }
    if (opts->rename_project > 0 && opts->rename_project_to != NULL)
    {
        if (nt_model_rename_project(model, opts->rename_project, opts->rename_project_to) == 0)
            printf("Renamed project %d to: %s\n", opts->rename_project, opts->rename_project_to);
        else
            fprintf(stderr, "Error renaming project\n");
        return 1;
    }
    return 0;
}

static int i_handle_annotations(NtModel *model, const Opts *opts)
{
    if (opts->annotate != NULL && opts->annotate_task > 0)
    {
        nt_model_add_annotation(model, opts->annotate_task, opts->annotate);
        printf("Added annotation to task %d\n", opts->annotate_task);
        return 1;
    }
    if (opts->list_annotations > 0)
    {
        NtAnnotation *annotations[512];
        size_t count = nt_model_task_annotations(model, opts->list_annotations, annotations, 512);
        size_t i;
        if (opts->json)
        {
            puts("[");
            for (i = 0; i < count; i += 1)
            {
                char when[32];
                nt_format_datetime(annotations[i]->created_at, when, sizeof(when));
                printf("  {\"date\": "); i_json_escape(when);
                printf(", \"text\": "); i_json_escape(annotations[i]->text);
                printf("}%s\n", i + 1 < count ? "," : "");
            }
            puts("]");
        }
        else
        {
            printf("Annotations for task %d:\n", opts->list_annotations);
            if (count == 0)
                puts("  (no annotations)");
            for (i = 0; i < count; i += 1)
            {
                char when[32];
                nt_format_datetime(annotations[i]->created_at, when, sizeof(when));
                printf("  [%s] %s\n", when, annotations[i]->text);
            }
        }
        return 1;
    }
    if (opts->note != NULL)
    {
        size_t i;
        for (i = 0; i < model->ntasks; i += 1)
        {
            if (model->tasks[i].timing)
            {
                nt_model_add_annotation(model, model->tasks[i].id, opts->note);
                printf("Added note to task %d\n", model->tasks[i].id);
                return 1;
            }
        }
        fprintf(stderr, "No task is currently running\n");
        return 1;
    }
    return 0;
}

static int i_handle_tags(NtModel *model, const Opts *opts)
{
    if (opts->add_tag != NULL && opts->add_tag_task > 0)
    {
        nt_model_add_tag(model, opts->add_tag_task, opts->add_tag);
        printf("Added tag '%s' to task %d\n", opts->add_tag, opts->add_tag_task);
        return 1;
    }
    if (opts->remove_tag != NULL && opts->remove_tag_task > 0)
    {
        nt_model_remove_tag(model, opts->remove_tag_task, opts->remove_tag);
        printf("Removed tag '%s' from task %d\n", opts->remove_tag, opts->remove_tag_task);
        return 1;
    }
    if (opts->list_tags)
    {
        size_t i;
        if (opts->json)
        {
            puts("[");
            for (i = 0; i < model->ntags; i += 1)
            {
                printf("  ");
                i_json_escape(model->tags[i].name);
                printf("%s\n", i + 1 < model->ntags ? "," : "");
            }
            puts("]");
        }
        else
        {
            for (i = 0; i < model->ntags; i += 1)
                puts(model->tags[i].name);
            printf("\n%zu tags total.\n", model->ntags);
        }
        return 1;
    }
    return 0;
}

static int i_handle_data(NtModel *model, const Opts *opts, const char *db_path)
{
    if (opts->export_csv != NULL)
    {
        if (nt_model_export_csv(model, opts->export_csv) == 0)
            printf("Exported %zu tasks to: %s\n", model->ntasks, opts->export_csv);
        else
            fprintf(stderr, "Error: Failed to write CSV to %s\n", opts->export_csv);
        return 1;
    }
    if (opts->export_json != NULL)
    {
        if (nt_model_export_json(model, opts->export_json) == 0)
            printf("Exported %zu tasks to: %s\n", model->ntasks, opts->export_json);
        else
            fprintf(stderr, "Error: Failed to write JSON to %s\n", opts->export_json);
        return 1;
    }
    if (opts->import_csv != NULL)
    {
        char error[256];
        int imported = nt_model_import_csv(model, opts->import_csv, error, sizeof(error));
        if (imported < 0)
            fprintf(stderr, "Error: %s\n", error[0] != '\0' ? error : "CSV import failed");
        else
            printf("Imported %d tasks from: %s\n", imported, opts->import_csv);
        return 1;
    }
    if (opts->backup != NULL)
    {
        char error[256];
        error[0] = '\0';
        if (nt_store_backup(model->store, opts->backup, error, sizeof(error)) == 0)
            printf("Database backed up to: %s\n", opts->backup);
        else
            fprintf(stderr, "Backup failed: %s\n", error[0] != '\0' ? error : "unknown error");
        return 1;
    }
    if (opts->restore != NULL)
    {
        char error[256];
        error[0] = '\0';
        if (nt_store_restore(model->store, opts->restore, error, sizeof(error)) == 0)
            printf("Database restored from: %s\n", opts->restore);
        else
            fprintf(stderr, "Restore failed: %s\n", error[0] != '\0' ? error : "unknown error");
        return 1;
    }
    if (opts->export_sqlite != NULL)
    {
        char error[256];
        error[0] = '\0';
        if (nt_store_backup(model->store, opts->export_sqlite, error, sizeof(error)) == 0)
            printf("SQLite database exported to: %s\n", opts->export_sqlite);
        else
            fprintf(stderr, "Export failed: %s\n", error[0] != '\0' ? error : "unknown error");
        return 1;
    }
    (void)db_path;
    return 0;
}

static int i_handle_utility(NtModel *model, const Opts *opts)
{
    if (opts->total_time)
    {
        int64_t total = 0;
        size_t i;
        for (i = 0; i < model->ntasks; i += 1)
            total += nt_model_total_seconds(model, model->tasks[i].id);
        if (opts->json)
            printf("{\"total_seconds\": %lld}\n", (long long)total);
        else
            printf("Total time tracked: %lld hours %lld minutes\n", (long long)(total / 3600), (long long)((total % 3600) / 60));
        return 1;
    }
    if (opts->active_time)
    {
        time_t now = time(NULL);
        size_t i;
        int first = 1;
        if (opts->json)
            puts("[");
        for (i = 0; i < model->ntasks; i += 1)
        {
            NtTask *task = &model->tasks[i];
            int64_t elapsed;
            char duration[32];
            if (!task->timing)
                continue;
            elapsed = nt_timer_elapsed(model, task, now);
            nt_format_duration(elapsed, duration, sizeof(duration));
            if (opts->json)
            {
                if (!first)
                    puts(",");
                first = 0;
                printf("  {\"id\": %d, \"name\": ", task->id);
                i_json_escape(task->name);
                printf(", \"elapsed_seconds\": %lld}", (long long)elapsed);
            }
            else
            {
                printf("%s: %s\n", task->name, duration);
            }
        }
        if (opts->json)
            puts(first ? "]" : "\n]");
        return 1;
    }
    if (opts->summary)
    {
        size_t i;
        int running = 0;
        int64_t today_total = 0;
        char today[NT_DATE_LEN];
        nt_today(today);
        for (i = 0; i < model->ntasks; i += 1)
        {
            if (model->tasks[i].timing)
                running += 1;
            today_total += nt_model_day_seconds(model, model->tasks[i].id, today);
        }
        if (opts->json)
            printf("{\"running\": %d, \"total_tasks\": %zu, \"today_seconds\": %lld}\n", running, model->ntasks, (long long)today_total);
        else
            printf("%d tasks running, %lld:%02lld today\n", running, (long long)(today_total / 3600), (long long)((today_total % 3600) / 60));
        return 1;
    }
    if (opts->merge_source > 0)
    {
        fprintf(stderr, "Merge task not yet implemented (requires db function)\n");
        return 1;
    }
    if (opts->vacuum)
    {
        char error[256];
        error[0] = '\0';
        if (nt_store_vacuum(model->store, error, sizeof(error)) == 0)
            puts("Database compacted");
        else
            fprintf(stderr, "Vacuum failed: %s\n", error[0] != '\0' ? error : "unknown error");
        return 1;
    }
    return 0;
}

int nt_cli_is_cli(int argc, char *argv[])
{
    Opts opts;
    i_parse(argc, argv, &opts);
    return i_any_cli(&opts);
}

int nt_cli_dispatch(int argc, char *argv[], const char *db_path)
{
    Opts opts;
    NtStore store;
    NtModel model;
    char error[256];

    i_parse(argc, argv, &opts);
    if (!i_any_cli(&opts))
        return 0;

    if (!opts.quiet)
        printf("Naptimer %s\n", NAPTIMER_VERSION);

    error[0] = '\0';
    if (nt_store_open(db_path, &store, error, sizeof(error)) != 0)
    {
        fprintf(stderr, "Failed to initialize database: %s\n", error[0] != '\0' ? error : "unknown error");
        return 1;
    }
    nt_model_init(&model, &store);
    if (nt_model_load(&model, error, sizeof(error)) != 0)
    {
        fprintf(stderr, "Failed to load database: %s\n", error[0] != '\0' ? error : "unknown error");
        nt_store_close(&store);
        return 1;
    }

    if (opts.list_tasks)
        i_list_tasks(&model, &opts);
    else if (opts.show_status)
        i_print_status(&model, opts.json);
    else if (i_handle_timer(&model, &opts)) { }
    else if (i_handle_task_management(&model, &opts)) { }
    else if (i_handle_reports(&model, &opts)) { }
    else if (i_handle_projects(&model, &opts)) { }
    else if (i_handle_annotations(&model, &opts)) { }
    else if (i_handle_tags(&model, &opts)) { }
    else if (i_handle_data(&model, &opts, db_path)) { }
    else if (i_handle_utility(&model, &opts)) { }

    nt_model_save_session(&model);
    nt_model_cleanup(&model);
    nt_store_close(&store);
    return 0;
}
