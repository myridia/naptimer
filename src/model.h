#ifndef NAPTIMER_MODEL_H
#define NAPTIMER_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include "store.h"
#include "util.h"

#define NT_NAME_LEN 160
#define NT_TAGS_LEN 512
#define NT_TEXT_LEN 2048

typedef struct
{
    int id;
    char name[NT_NAME_LEN];
    time_t created_at;
} NtProject;

typedef struct
{
    int id;
    char name[NT_NAME_LEN];
    int project_id;
    int hidden;
    int timing;
    time_t last_start;
    time_t created_at;
    char tags[NT_TAGS_LEN];
    time_t paused_at;
    int64_t paused_secs;
} NtTask;

typedef struct
{
    int task_id;
    char date[NT_DATE_LEN];
    int64_t seconds;
} NtDay;

typedef struct
{
    int id;
    int task_id;
    time_t created_at;
    char text[NT_TEXT_LEN];
} NtAnnotation;

typedef struct
{
    int id;
    char name[64];
} NtTag;

typedef struct
{
    int width;
    int height;
    int auto_save;
    int animate;
    int resume;
    int idle_enabled;
    int idle_threshold;
    int midnight_offset;
    int week_start;
    int reminder;
    int reminder_interval;
    int next_id;
    int ui_font_size;
} NtSettings;

typedef struct
{
    char view_date[NT_DATE_LEN];
    int64_t buffer;
    time_t idle_since;
    int idle_pending;
} NtSession;

typedef struct
{
    NtStore *store;
    NtProject *projects;
    size_t nprojects;
    size_t cap_projects;
    NtTask *tasks;
    size_t ntasks;
    size_t cap_tasks;
    NtDay *days;
    size_t ndays;
    size_t cap_days;
    NtAnnotation *annotations;
    size_t nannotations;
    size_t cap_annotations;
    NtTag *tags;
    size_t ntags;
    size_t cap_tags;
    NtSettings settings;
    NtSession session;
} NtModel;

void nt_model_init(NtModel *model, NtStore *store);
int nt_model_load(NtModel *model, char *error, size_t error_size);
void nt_model_cleanup(NtModel *model);

void nt_model_save_settings(NtModel *model);
void nt_model_save_session(NtModel *model);
int nt_model_next_id(NtModel *model);

NtProject *nt_model_project(NtModel *model, int id);
const char *nt_model_project_name(const NtModel *model, int id);
NtTask *nt_model_task(NtModel *model, int id);
NtAnnotation *nt_model_annotation(NtModel *model, int id);
NtProject *nt_model_add_project(NtModel *model, const char *name);
int nt_model_rename_project(NtModel *model, int id, const char *name);
int nt_model_delete_project(NtModel *model, int id);
NtTask *nt_model_add_task(NtModel *model, const char *name, int project_id, const char *tags);
int nt_model_update_task(NtModel *model, int id, const char *name, int project_id, const char *tags);
int nt_model_delete_task(NtModel *model, int id);
int nt_model_hide_task(NtModel *model, int id, int hidden);
void nt_model_save_task(NtModel *model, NtTask *task);

void nt_model_add_time(NtModel *model, int task_id, const char *date, int64_t seconds);
void nt_model_set_time(NtModel *model, int task_id, const char *date, int64_t seconds);
int64_t nt_model_day_seconds(const NtModel *model, int task_id, const char *date);
int64_t nt_model_total_seconds(const NtModel *model, int task_id);
int64_t nt_model_flush_elapsed(NtModel *model, int task_id, time_t from, time_t to);
void nt_model_reset_time(NtModel *model, int task_id);

NtAnnotation *nt_model_add_annotation(NtModel *model, int task_id, const char *text);
size_t nt_model_task_annotations(const NtModel *model, int task_id, NtAnnotation **out, size_t max);

int nt_model_ensure_tag(NtModel *model, const char *name);
int nt_model_add_tag(NtModel *model, int task_id, const char *name);
int nt_model_remove_tag(NtModel *model, int task_id, const char *name);
int nt_model_task_has_tag(const NtModel *model, int task_id, const char *name);
void nt_model_sync_tags(NtModel *model, int task_id, const char *comma_separated);
void nt_model_tags_string(const NtModel *model, int task_id, char *out, size_t size);

int nt_model_export_json(const NtModel *model, const char *path);
int nt_model_export_csv(const NtModel *model, const char *path);
int nt_model_import_csv(NtModel *model, const char *path, char *error, size_t error_size);

#endif
