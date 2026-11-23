#include "model.h"

#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void *i_grow(void *ptr, size_t *capacity, size_t count, size_t element)
{
    size_t next;
    void *data;

    if (count < *capacity)
        return ptr;
    next = *capacity == 0 ? 8 : *capacity * 2;
    data = realloc(ptr, next * element);
    if (data != NULL)
        *capacity = next;
    return data;
}

static int i_name_cmp(const char *a, const char *b)
{
    int ca;
    int cb;
    while (*a != '\0' && *b != '\0')
    {
        ca = tolower((unsigned char)*a);
        cb = tolower((unsigned char)*b);
        if (ca != cb)
            return ca < cb ? -1 : 1;
        a += 1;
        b += 1;
    }
    if (*a == *b)
        return 0;
    return *a == '\0' ? -1 : 1;
}

static void i_doc_id(char *out, size_t size, const char *kind, int id)
{
    snprintf(out, size, "%s-%d", kind, id);
}

static void i_day_id(char *out, size_t size, int task_id, const char *date)
{
    snprintf(out, size, "day-%d-%s", task_id, date);
}

static void i_save(NtModel *model, const char *id, NtJBuf *buf)
{
    char *doc;

    if (model == NULL || model->store == NULL)
    {
        nt_jb_free(buf);
        return;
    }
    doc = nt_jb_take(buf);
    nt_store_put(model->store, id, doc, NULL, 0);
    free(doc);
    nt_jb_free(buf);
}

static void i_begin_doc(NtJBuf *buf, const char *type)
{
    nt_jb_init(buf);
    nt_jb_puts(buf, "{");
    nt_jb_puts(buf, "\"type\":");
    nt_jb_str(buf, type);
}

static void i_end_doc(NtJBuf *buf)
{
    nt_jb_puts(buf, "}");
}

void nt_model_save_settings(NtModel *model)
{
    NtJBuf buf;
    NtSettings *s;

    if (model == NULL)
        return;
    s = &model->settings;
    i_begin_doc(&buf, "settings");
    nt_jb_puts(&buf, ",\"width\":"); nt_jb_int(&buf, s->width);
    nt_jb_puts(&buf, ",\"height\":"); nt_jb_int(&buf, s->height);
    nt_jb_puts(&buf, ",\"auto_save\":"); nt_jb_bool(&buf, s->auto_save);
    nt_jb_puts(&buf, ",\"animate\":"); nt_jb_bool(&buf, s->animate);
    nt_jb_puts(&buf, ",\"resume\":"); nt_jb_bool(&buf, s->resume);
    nt_jb_puts(&buf, ",\"idle_enabled\":"); nt_jb_bool(&buf, s->idle_enabled);
    nt_jb_puts(&buf, ",\"idle_threshold\":"); nt_jb_int(&buf, s->idle_threshold);
    nt_jb_puts(&buf, ",\"midnight_offset\":"); nt_jb_int(&buf, s->midnight_offset);
    nt_jb_puts(&buf, ",\"week_start\":"); nt_jb_int(&buf, s->week_start);
    nt_jb_puts(&buf, ",\"reminder\":"); nt_jb_bool(&buf, s->reminder);
    nt_jb_puts(&buf, ",\"reminder_interval\":"); nt_jb_int(&buf, s->reminder_interval);
    nt_jb_puts(&buf, ",\"next_id\":"); nt_jb_int(&buf, s->next_id);
    nt_jb_puts(&buf, ",\"ui_font_size\":"); nt_jb_int(&buf, s->ui_font_size);
    nt_jb_puts(&buf, ",\"col_w0\":"); nt_jb_int(&buf, s->col_widths[0]);
    nt_jb_puts(&buf, ",\"col_w1\":"); nt_jb_int(&buf, s->col_widths[1]);
    nt_jb_puts(&buf, ",\"col_w2\":"); nt_jb_int(&buf, s->col_widths[2]);
    nt_jb_puts(&buf, ",\"col_w3\":"); nt_jb_int(&buf, s->col_widths[3]);
    nt_jb_puts(&buf, ",\"col_w4\":"); nt_jb_int(&buf, s->col_widths[4]);
    i_end_doc(&buf);
    i_save(model, "settings", &buf);
}

void nt_model_save_session(NtModel *model)
{
    NtJBuf buf;

    if (model == NULL)
        return;
    i_begin_doc(&buf, "session");
    nt_jb_puts(&buf, ",\"view_date\":"); nt_jb_str(&buf, model->session.view_date);
    nt_jb_puts(&buf, ",\"buffer\":"); nt_jb_int(&buf, model->session.buffer);
    i_end_doc(&buf);
    i_save(model, "session", &buf);
}

static void i_save_project(NtModel *model, const NtProject *project)
{
    NtJBuf buf;
    char id[64];

    i_begin_doc(&buf, "project");
    nt_jb_puts(&buf, ",\"id\":"); nt_jb_int(&buf, project->id);
    nt_jb_puts(&buf, ",\"name\":"); nt_jb_str(&buf, project->name);
    nt_jb_puts(&buf, ",\"created_at\":"); nt_jb_int(&buf, (int64_t)project->created_at);
    i_end_doc(&buf);
    i_doc_id(id, sizeof(id), "project", project->id);
    i_save(model, id, &buf);
}

static void i_save_task(NtModel *model, const NtTask *task)
{
    NtJBuf buf;
    char id[64];

    i_begin_doc(&buf, "task");
    nt_jb_puts(&buf, ",\"id\":"); nt_jb_int(&buf, task->id);
    nt_jb_puts(&buf, ",\"name\":"); nt_jb_str(&buf, task->name);
    nt_jb_puts(&buf, ",\"project_id\":"); nt_jb_int(&buf, task->project_id);
    nt_jb_puts(&buf, ",\"hidden\":"); nt_jb_bool(&buf, task->hidden);
    nt_jb_puts(&buf, ",\"timing\":"); nt_jb_bool(&buf, task->timing);
    nt_jb_puts(&buf, ",\"last_start\":"); nt_jb_int(&buf, (int64_t)task->last_start);
    nt_jb_puts(&buf, ",\"created_at\":"); nt_jb_int(&buf, (int64_t)task->created_at);
    nt_jb_puts(&buf, ",\"tags\":"); nt_jb_str(&buf, task->tags);
    i_end_doc(&buf);
    i_doc_id(id, sizeof(id), "task", task->id);
    i_save(model, id, &buf);
}

static void i_save_day(NtModel *model, const NtDay *day)
{
    NtJBuf buf;
    char id[80];

    i_begin_doc(&buf, "day");
    nt_jb_puts(&buf, ",\"task_id\":"); nt_jb_int(&buf, day->task_id);
    nt_jb_puts(&buf, ",\"date\":"); nt_jb_str(&buf, day->date);
    nt_jb_puts(&buf, ",\"seconds\":"); nt_jb_int(&buf, day->seconds);
    i_end_doc(&buf);
    i_day_id(id, sizeof(id), day->task_id, day->date);
    i_save(model, id, &buf);
}

static void i_save_annotation(NtModel *model, const NtAnnotation *annotation)
{
    NtJBuf buf;
    char id[64];

    i_begin_doc(&buf, "annotation");
    nt_jb_puts(&buf, ",\"id\":"); nt_jb_int(&buf, annotation->id);
    nt_jb_puts(&buf, ",\"task_id\":"); nt_jb_int(&buf, annotation->task_id);
    nt_jb_puts(&buf, ",\"created_at\":"); nt_jb_int(&buf, (int64_t)annotation->created_at);
    nt_jb_puts(&buf, ",\"text\":"); nt_jb_str(&buf, annotation->text);
    i_end_doc(&buf);
    i_doc_id(id, sizeof(id), "annotation", annotation->id);
    i_save(model, id, &buf);
}

static void i_save_tag(NtModel *model, const NtTag *tag)
{
    NtJBuf buf;
    char id[64];

    i_begin_doc(&buf, "tag");
    nt_jb_puts(&buf, ",\"id\":"); nt_jb_int(&buf, tag->id);
    nt_jb_puts(&buf, ",\"name\":"); nt_jb_str(&buf, tag->name);
    i_end_doc(&buf);
    i_doc_id(id, sizeof(id), "tag", tag->id);
    i_save(model, id, &buf);
}

typedef struct
{
    NtModel *model;
    int count;
    int max_id;
} LoadCtx;

static void i_load_cb(const char *id, const char *rev, const char *doc, void *context)
{
    LoadCtx *ctx = (LoadCtx *)context;
    NtModel *model = ctx->model;
    NtJson *json;
    const char *type;

    (void)id;
    (void)rev;
    json = nt_json_parse(doc, NULL, 0);
    if (json == NULL)
        return;
    ctx->count += 1;
    type = nt_json_str(nt_json_get(json, "type"), "");
    if (strcmp(type, "settings") == 0)
    {
        NtSettings *s = &model->settings;
        s->width = (int)nt_json_int(nt_json_get(json, "width"), 900);
        s->height = (int)nt_json_int(nt_json_get(json, "height"), 600);
        s->auto_save = nt_json_bool(nt_json_get(json, "auto_save"), 1);
        s->animate = nt_json_bool(nt_json_get(json, "animate"), 1);
        s->resume = nt_json_bool(nt_json_get(json, "resume"), 0);
        s->idle_enabled = nt_json_bool(nt_json_get(json, "idle_enabled"), 1);
        s->idle_threshold = (int)nt_json_int(nt_json_get(json, "idle_threshold"), 15);
        s->midnight_offset = (int)nt_json_int(nt_json_get(json, "midnight_offset"), 0);
        s->week_start = (int)nt_json_int(nt_json_get(json, "week_start"), 1);
        s->reminder = nt_json_bool(nt_json_get(json, "reminder"), 0);
        s->reminder_interval = (int)nt_json_int(nt_json_get(json, "reminder_interval"), 15);
        s->next_id = (int)nt_json_int(nt_json_get(json, "next_id"), 0);
        s->ui_font_size = (int)nt_json_int(nt_json_get(json, "ui_font_size"), 0);
        s->col_widths[0] = (int)nt_json_int(nt_json_get(json, "col_w0"), 0);
        s->col_widths[1] = (int)nt_json_int(nt_json_get(json, "col_w1"), 0);
        s->col_widths[2] = (int)nt_json_int(nt_json_get(json, "col_w2"), 0);
        s->col_widths[3] = (int)nt_json_int(nt_json_get(json, "col_w3"), 0);
        s->col_widths[4] = (int)nt_json_int(nt_json_get(json, "col_w4"), 0);
    }
    else if (strcmp(type, "session") == 0)
    {
        snprintf(model->session.view_date, NT_DATE_LEN, "%s",
                 nt_json_str(nt_json_get(json, "view_date"), ""));
        model->session.buffer = nt_json_int(nt_json_get(json, "buffer"), 0);
    }
    else if (strcmp(type, "project") == 0)
    {
        NtProject *p;
        int item_id = (int)nt_json_int(nt_json_get(json, "id"), 0);
        if (nt_model_project(model, item_id) != NULL)
        {
            nt_json_free(json);
            return;
        }
        model->projects = i_grow(model->projects, &model->cap_projects,
                                 model->nprojects, sizeof(NtProject));
        p = &model->projects[model->nprojects++];
        memset(p, 0, sizeof(*p));
        p->id = item_id;
        snprintf(p->name, sizeof(p->name), "%s", nt_json_str(nt_json_get(json, "name"), ""));
        p->created_at = (time_t)nt_json_int(nt_json_get(json, "created_at"), 0);
        if (item_id > ctx->max_id)
            ctx->max_id = item_id;
    }
    else if (strcmp(type, "task") == 0)
    {
        NtTask *task;
        int item_id = (int)nt_json_int(nt_json_get(json, "id"), 0);
        if (nt_model_task(model, item_id) != NULL)
        {
            nt_json_free(json);
            return;
        }
        model->tasks = i_grow(model->tasks, &model->cap_tasks, model->ntasks, sizeof(NtTask));
        task = &model->tasks[model->ntasks++];
        memset(task, 0, sizeof(*task));
        task->id = item_id;
        snprintf(task->name, sizeof(task->name), "%s", nt_json_str(nt_json_get(json, "name"), ""));
        task->project_id = (int)nt_json_int(nt_json_get(json, "project_id"), -1);
        task->hidden = nt_json_bool(nt_json_get(json, "hidden"), 0);
        task->timing = nt_json_bool(nt_json_get(json, "timing"), 0);
        task->last_start = (time_t)nt_json_int(nt_json_get(json, "last_start"), 0);
        task->created_at = (time_t)nt_json_int(nt_json_get(json, "created_at"), 0);
        snprintf(task->tags, sizeof(task->tags), "%s", nt_json_str(nt_json_get(json, "tags"), ""));
        if (item_id > ctx->max_id)
            ctx->max_id = item_id;
    }
    else if (strcmp(type, "day") == 0)
    {
        NtDay *day;
        int task_id = (int)nt_json_int(nt_json_get(json, "task_id"), 0);
        char date[NT_DATE_LEN];
        snprintf(date, sizeof(date), "%s", nt_json_str(nt_json_get(json, "date"), ""));
        model->days = i_grow(model->days, &model->cap_days, model->ndays, sizeof(NtDay));
        day = &model->days[model->ndays++];
        day->task_id = task_id;
        snprintf(day->date, NT_DATE_LEN, "%s", date);
        day->seconds = nt_json_int(nt_json_get(json, "seconds"), 0);
    }
    else if (strcmp(type, "annotation") == 0)
    {
        NtAnnotation *annotation;
        int item_id = (int)nt_json_int(nt_json_get(json, "id"), 0);
        model->annotations = i_grow(model->annotations, &model->cap_annotations,
                                    model->nannotations, sizeof(NtAnnotation));
        annotation = &model->annotations[model->nannotations++];
        memset(annotation, 0, sizeof(*annotation));
        annotation->id = item_id;
        annotation->task_id = (int)nt_json_int(nt_json_get(json, "task_id"), 0);
        annotation->created_at = (time_t)nt_json_int(nt_json_get(json, "created_at"), 0);
        snprintf(annotation->text, sizeof(annotation->text), "%s",
                 nt_json_str(nt_json_get(json, "text"), ""));
        if (item_id > ctx->max_id)
            ctx->max_id = item_id;
    }
    else if (strcmp(type, "tag") == 0)
    {
        NtTag *tag;
        int item_id = (int)nt_json_int(nt_json_get(json, "id"), 0);
        model->tags = i_grow(model->tags, &model->cap_tags, model->ntags, sizeof(NtTag));
        tag = &model->tags[model->ntags++];
        tag->id = item_id;
        snprintf(tag->name, sizeof(tag->name), "%s", nt_json_str(nt_json_get(json, "name"), ""));
        if (item_id > ctx->max_id)
            ctx->max_id = item_id;
    }
    nt_json_free(json);
}

void nt_model_init(NtModel *model, NtStore *store)
{
    memset(model, 0, sizeof(*model));
    model->store = store;
    model->settings.width = 900;
    model->settings.height = 460;
    model->settings.auto_save = 1;
    model->settings.animate = 1;
    model->settings.idle_enabled = 1;
    model->settings.idle_threshold = 15;
    model->settings.week_start = 1;
    model->settings.reminder_interval = 15;
    model->settings.next_id = 1;
    model->settings.ui_font_size = 0;
}

int nt_model_load(NtModel *model, char *error, size_t error_size)
{
    LoadCtx ctx;
    size_t i;

    ctx.model = model;
    ctx.count = 0;
    ctx.max_id = 0;
    if (nt_store_each(model->store, i_load_cb, &ctx, error, error_size) != 0)
        return -1;
    if (model->settings.next_id <= ctx.max_id)
        model->settings.next_id = ctx.max_id + 1;
    if (ctx.count == 0)
    {
        nt_model_save_settings(model);
        nt_model_save_session(model);
    }
    else if (nt_store_get(model->store, "settings", NULL, NULL) != 0)
    {
        nt_model_save_settings(model);
    }
    else if (nt_store_get(model->store, "session", NULL, NULL) != 0)
    {
        nt_model_save_session(model);
    }
    for (i = 0; i < model->nprojects; i += 1)
        i_save_project(model, &model->projects[i]);
    for (i = 0; i < model->ntasks; i += 1)
        i_save_task(model, &model->tasks[i]);
    for (i = 0; i < model->ndays; i += 1)
        i_save_day(model, &model->days[i]);
    for (i = 0; i < model->nannotations; i += 1)
        i_save_annotation(model, &model->annotations[i]);
    for (i = 0; i < model->ntags; i += 1)
        i_save_tag(model, &model->tags[i]);
    return 0;
}

void nt_model_cleanup(NtModel *model)
{
    free(model->projects);
    free(model->tasks);
    free(model->days);
    free(model->annotations);
    free(model->tags);
    memset(model, 0, sizeof(*model));
}

int nt_model_next_id(NtModel *model)
{
    int id = model->settings.next_id++;
    nt_model_save_settings(model);
    return id;
}

NtProject *nt_model_project(NtModel *model, int id)
{
    size_t i;
    for (i = 0; i < model->nprojects; i += 1)
    {
        if (model->projects[i].id == id)
            return &model->projects[i];
    }
    return NULL;
}

const char *nt_model_project_name(const NtModel *model, int id)
{
    size_t i;
    for (i = 0; i < model->nprojects; i += 1)
    {
        if (model->projects[i].id == id)
            return model->projects[i].name;
    }
    return "";
}

NtTask *nt_model_task(NtModel *model, int id)
{
    size_t i;
    for (i = 0; i < model->ntasks; i += 1)
    {
        if (model->tasks[i].id == id)
            return &model->tasks[i];
    }
    return NULL;
}

NtAnnotation *nt_model_annotation(NtModel *model, int id)
{
    size_t i;
    for (i = 0; i < model->nannotations; i += 1)
    {
        if (model->annotations[i].id == id)
            return &model->annotations[i];
    }
    return NULL;
}

NtProject *nt_model_add_project(NtModel *model, const char *name)
{
    NtProject *project;

    model->projects = i_grow(model->projects, &model->cap_projects,
                             model->nprojects, sizeof(NtProject));
    project = &model->projects[model->nprojects++];
    memset(project, 0, sizeof(*project));
    project->id = nt_model_next_id(model);
    snprintf(project->name, sizeof(project->name), "%s", name != NULL ? name : "");
    project->created_at = time(NULL);
    i_save_project(model, project);
    return project;
}

int nt_model_rename_project(NtModel *model, int id, const char *name)
{
    NtProject *project = nt_model_project(model, id);
    if (project == NULL)
        return -1;
    snprintf(project->name, sizeof(project->name), "%s", name != NULL ? name : "");
    i_save_project(model, project);
    return 0;
}

int nt_model_delete_project(NtModel *model, int id)
{
    size_t i;
    char doc_id[64];

    if (nt_model_project(model, id) == NULL)
        return -1;
    for (i = 0; i < model->ntasks; i += 1)
    {
        if (model->tasks[i].project_id == id)
        {
            model->tasks[i].project_id = -1;
            i_save_task(model, &model->tasks[i]);
        }
    }
    for (i = 0; i < model->nprojects; i += 1)
    {
        if (model->projects[i].id == id)
        {
            memmove(&model->projects[i], &model->projects[i + 1],
                    (model->nprojects - i - 1) * sizeof(NtProject));
            model->nprojects -= 1;
            break;
        }
    }
    i_doc_id(doc_id, sizeof(doc_id), "project", id);
    nt_store_del(model->store, doc_id);
    return 0;
}

NtTask *nt_model_add_task(NtModel *model, const char *name, int project_id, const char *tags)
{
    NtTask *task;

    model->tasks = i_grow(model->tasks, &model->cap_tasks, model->ntasks, sizeof(NtTask));
    task = &model->tasks[model->ntasks++];
    memset(task, 0, sizeof(*task));
    task->id = nt_model_next_id(model);
    snprintf(task->name, sizeof(task->name), "%s", name != NULL ? name : "");
    task->project_id = project_id;
    task->created_at = time(NULL);
    if (tags != NULL && tags[0] != '\0')
        nt_model_sync_tags(model, task->id, tags);
    i_save_task(model, task);
    return nt_model_task(model, task->id);
}

int nt_model_update_task(NtModel *model, int id, const char *name, int project_id, const char *tags)
{
    NtTask *task = nt_model_task(model, id);
    char normalized[NT_TAGS_LEN];

    if (task == NULL)
        return -1;
    snprintf(task->name, sizeof(task->name), "%s", name != NULL ? name : "");
    task->project_id = project_id;
    if (tags != NULL)
    {
        snprintf(normalized, sizeof(normalized), "%s", tags);
        nt_model_sync_tags(model, id, normalized);
    }
    task = nt_model_task(model, id);
    i_save_task(model, task);
    return 0;
}

int nt_model_delete_task(NtModel *model, int id)
{
    size_t i;
    char doc_id[80];

    if (nt_model_task(model, id) == NULL)
        return -1;
    for (i = 0; i < model->ndays; i += 1)
    {
        if (model->days[i].task_id == id)
        {
            i_day_id(doc_id, sizeof(doc_id), id, model->days[i].date);
            nt_store_del(model->store, doc_id);
        }
    }
    for (i = 0; i < model->nannotations; i += 1)
    {
        if (model->annotations[i].task_id == id)
        {
            snprintf(doc_id, sizeof(doc_id), "annotation-%d", model->annotations[i].id);
            nt_store_del(model->store, doc_id);
        }
    }
    for (i = 0; i < model->ndays;)
    {
        if (model->days[i].task_id == id)
        {
            memmove(&model->days[i], &model->days[i + 1],
                    (model->ndays - i - 1) * sizeof(NtDay));
            model->ndays -= 1;
        }
        else
            i += 1;
    }
    for (i = 0; i < model->nannotations;)
    {
        if (model->annotations[i].task_id == id)
        {
            memmove(&model->annotations[i], &model->annotations[i + 1],
                    (model->nannotations - i - 1) * sizeof(NtAnnotation));
            model->nannotations -= 1;
        }
        else
            i += 1;
    }
    for (i = 0; i < model->ntasks; i += 1)
    {
        if (model->tasks[i].id == id)
        {
            memmove(&model->tasks[i], &model->tasks[i + 1],
                    (model->ntasks - i - 1) * sizeof(NtTask));
            model->ntasks -= 1;
            break;
        }
    }
    snprintf(doc_id, sizeof(doc_id), "task-%d", id);
    nt_store_del(model->store, doc_id);
    return 0;
}

int nt_model_hide_task(NtModel *model, int id, int hidden)
{
    NtTask *task = nt_model_task(model, id);
    if (task == NULL)
        return -1;
    task->hidden = hidden ? 1 : 0;
    i_save_task(model, task);
    return 0;
}

void nt_model_save_task(NtModel *model, NtTask *task)
{
    if (task != NULL)
        i_save_task(model, task);
}

void nt_model_add_time(NtModel *model, int task_id, const char *date, int64_t seconds)
{
    NtDay *day = NULL;
    size_t i;
    char doc_id[80];

    if (date == NULL || !nt_date_valid(date))
        return;
    for (i = 0; i < model->ndays; i += 1)
    {
        if (model->days[i].task_id == task_id && strcmp(model->days[i].date, date) == 0)
        {
            day = &model->days[i];
            break;
        }
    }
    if (day == NULL)
    {
        model->days = i_grow(model->days, &model->cap_days, model->ndays, sizeof(NtDay));
        day = &model->days[model->ndays++];
        memset(day, 0, sizeof(*day));
        day->task_id = task_id;
        snprintf(day->date, NT_DATE_LEN, "%s", date);
    }
    day->seconds += seconds;
    if (day->seconds < 0)
        day->seconds = 0;
    i_day_id(doc_id, sizeof(doc_id), task_id, date);
    if (day->seconds == 0)
    {
        nt_store_del(model->store, doc_id);
        memmove(day, day + 1, (model->ndays - (size_t)(day - model->days) - 1) * sizeof(NtDay));
        model->ndays -= 1;
    }
    else
    {
        i_save_day(model, day);
    }
}

void nt_model_set_time(NtModel *model, int task_id, const char *date, int64_t seconds)
{
    int64_t current = nt_model_day_seconds(model, task_id, date);
    nt_model_add_time(model, task_id, date, seconds - current);
}

int64_t nt_model_day_seconds(const NtModel *model, int task_id, const char *date)
{
    size_t i;
    const char *day_date = date;

    if (day_date == NULL || day_date[0] == '\0')
    {
        static char today[NT_DATE_LEN];
        nt_today(today);
        day_date = today;
    }
    for (i = 0; i < model->ndays; i += 1)
    {
        if (model->days[i].task_id == task_id && strcmp(model->days[i].date, day_date) == 0)
            return model->days[i].seconds;
    }
    return 0;
}

int64_t nt_model_total_seconds(const NtModel *model, int task_id)
{
    int64_t total = 0;
    size_t i;
    for (i = 0; i < model->ndays; i += 1)
    {
        if (model->days[i].task_id == task_id)
            total += model->days[i].seconds;
    }
    return total;
}

int64_t nt_model_flush_elapsed(NtModel *model, int task_id, time_t from, time_t to)
{
    time_t cursor = from;
    int64_t added = 0;
    int offset = model->settings.midnight_offset;
    int guard = 0;

    if (to <= from)
        return 0;
    while (cursor < to && guard < 4000)
    {
        char date[NT_DATE_LEN];
        time_t end;
        time_t segment_end;

        guard += 1;
        nt_date_of(cursor, offset, date);
        end = nt_day_after(date, offset);
        segment_end = end < to ? end : to;
        if (segment_end > cursor)
        {
            nt_model_add_time(model, task_id, date, segment_end - cursor);
            added += segment_end - cursor;
        }
        if (segment_end <= cursor)
            break;
        cursor = segment_end;
    }
    return added;
}

void nt_model_reset_time(NtModel *model, int task_id)
{
    size_t i;
    char doc_id[80];

    for (i = 0; i < model->ndays;)
    {
        if (model->days[i].task_id == task_id)
        {
            i_day_id(doc_id, sizeof(doc_id), task_id, model->days[i].date);
            nt_store_del(model->store, doc_id);
            memmove(&model->days[i], &model->days[i + 1],
                    (model->ndays - i - 1) * sizeof(NtDay));
            model->ndays -= 1;
        }
        else
            i += 1;
    }
}

NtAnnotation *nt_model_add_annotation(NtModel *model, int task_id, const char *text)
{
    NtAnnotation *annotation;

    if (text == NULL || text[0] == '\0')
        return NULL;
    model->annotations = i_grow(model->annotations, &model->cap_annotations,
                                model->nannotations, sizeof(NtAnnotation));
    annotation = &model->annotations[model->nannotations++];
    memset(annotation, 0, sizeof(*annotation));
    annotation->id = nt_model_next_id(model);
    annotation->task_id = task_id;
    annotation->created_at = time(NULL);
    snprintf(annotation->text, sizeof(annotation->text), "%s", text);
    i_save_annotation(model, annotation);
    return annotation;
}

size_t nt_model_task_annotations(const NtModel *model, int task_id, NtAnnotation **out, size_t max)
{
    size_t count = 0;
    size_t i;
    size_t j;

    for (i = 0; i < model->nannotations && count < max; i += 1)
    {
        if (model->annotations[i].task_id == task_id)
        {
            out[count++] = &model->annotations[i];
        }
    }
    for (i = 0; i < count; i += 1)
    {
        for (j = i + 1; j < count; j += 1)
        {
            if (out[j]->created_at > out[i]->created_at)
            {
                NtAnnotation *tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    return count;
}

int nt_model_ensure_tag(NtModel *model, const char *name)
{
    size_t i;
    NtTag *tag;
    char trimmed[64];

    if (name == NULL)
        return 0;
    snprintf(trimmed, sizeof(trimmed), "%s", name);
    nt_str_trim(trimmed);
    if (trimmed[0] == '\0')
        return 0;
    for (i = 0; i < model->ntags; i += 1)
    {
        if (strcmp(model->tags[i].name, trimmed) == 0)
            return model->tags[i].id;
    }
    model->tags = i_grow(model->tags, &model->cap_tags, model->ntags, sizeof(NtTag));
    tag = &model->tags[model->ntags++];
    tag->id = nt_model_next_id(model);
    snprintf(tag->name, sizeof(tag->name), "%s", trimmed);
    i_save_tag(model, tag);
    return tag->id;
}

int nt_model_task_has_tag(const NtModel *model, int task_id, const char *name)
{
    const NtTask *task = nt_model_task((NtModel *)model, task_id);
    char needle[NT_TAGS_LEN];
    char *copy;
    char *part;
    int found = 0;

    if (task == NULL || name == NULL)
        return 0;
    snprintf(needle, sizeof(needle), "%s", name);
    nt_str_trim(needle);
    copy = nt_strdup(task->tags);
    part = strtok(copy, ",");
    while (part != NULL)
    {
        nt_str_trim(part);
        if (strcmp(part, needle) == 0)
        {
            found = 1;
            break;
        }
        part = strtok(NULL, ",");
    }
    free(copy);
    return found;
}

int nt_model_add_tag(NtModel *model, int task_id, const char *name)
{
    NtTask *task = nt_model_task(model, task_id);
    char tag_name[64];

    if (task == NULL || name == NULL)
        return -1;
    snprintf(tag_name, sizeof(tag_name), "%s", name);
    nt_str_trim(tag_name);
    if (tag_name[0] == '\0' || nt_model_task_has_tag(model, task_id, tag_name))
        return 0;
    nt_model_ensure_tag(model, tag_name);
    if (task->tags[0] != '\0')
        snprintf(task->tags + strlen(task->tags), sizeof(task->tags) - strlen(task->tags),
                 ", %s", tag_name);
    else
        snprintf(task->tags, sizeof(task->tags), "%s", tag_name);
    i_save_task(model, nt_model_task(model, task_id));
    return 0;
}

int nt_model_remove_tag(NtModel *model, int task_id, const char *name)
{
    NtTask *task = nt_model_task(model, task_id);
    char copy[NT_TAGS_LEN];
    char *part;
    char result[NT_TAGS_LEN];
    size_t used = 0;

    if (task == NULL || name == NULL)
        return -1;
    snprintf(copy, sizeof(copy), "%s", task->tags);
    result[0] = '\0';
    part = strtok(copy, ",");
    while (part != NULL)
    {
        nt_str_trim(part);
        if (strcmp(part, name) != 0)
        {
            size_t len = strlen(part);
            if (used > 0)
            {
                if (used + 2 < sizeof(result))
                {
                    memcpy(result + used, ", ", 2);
                    used += 2;
                }
            }
            if (used + len < sizeof(result))
            {
                memcpy(result + used, part, len);
                used += len;
                result[used] = '\0';
            }
        }
        part = strtok(NULL, ",");
    }
    snprintf(task->tags, sizeof(task->tags), "%s", result);
    i_save_task(model, task);
    return 0;
}

void nt_model_sync_tags(NtModel *model, int task_id, const char *comma_separated)
{
    NtTask *task = nt_model_task(model, task_id);
    char copy[NT_TAGS_LEN];
    char result[NT_TAGS_LEN];
    char *part;
    size_t used = 0;

    if (task == NULL || comma_separated == NULL)
        return;
    snprintf(copy, sizeof(copy), "%s", comma_separated);
    result[0] = '\0';
    part = strtok(copy, ",");
    while (part != NULL)
    {
        nt_str_trim(part);
        if (part[0] != '\0')
        {
            nt_model_ensure_tag(model, part);
            if (used > 0 && used + 2 < sizeof(result))
            {
                memcpy(result + used, ", ", 2);
                used += 2;
            }
            if (used + strlen(part) < sizeof(result))
            {
                size_t len = strlen(part);
                memcpy(result + used, part, len);
                used += len;
                result[used] = '\0';
            }
        }
        part = strtok(NULL, ",");
    }
    task = nt_model_task(model, task_id);
    snprintf(task->tags, sizeof(task->tags), "%s", result);
    i_save_task(model, task);
}

void nt_model_tags_string(const NtModel *model, int task_id, char *out, size_t size)
{
    const NtTask *task = nt_model_task((NtModel *)model, task_id);
    snprintf(out, size, "%s", task != NULL ? task->tags : "");
}

int nt_model_export_json(const NtModel *model, const char *path)
{
    NtJBuf buf;
    char duration[32];
    size_t i;
    FILE *file;
    int result = -1;

    nt_jb_init(&buf);
    nt_jb_puts(&buf, "{\n  \"projects\": [");
    for (i = 0; i < model->nprojects; i += 1)
    {
        nt_jb_puts(&buf, i == 0 ? "\n    " : ",\n    ");
        nt_jb_puts(&buf, "{\"id\":"); nt_jb_int(&buf, model->projects[i].id);
        nt_jb_puts(&buf, ",\"name\":"); nt_jb_str(&buf, model->projects[i].name);
        nt_jb_puts(&buf, "}");
    }
    nt_jb_puts(&buf, "\n  ],\n  \"tasks\": [");
    for (i = 0; i < model->ntasks; i += 1)
    {
        const NtTask *task = &model->tasks[i];
        size_t j;
        nt_jb_puts(&buf, i == 0 ? "\n    " : ",\n    ");
        nt_jb_puts(&buf, "{\"id\":"); nt_jb_int(&buf, task->id);
        nt_jb_puts(&buf, ",\"name\":"); nt_jb_str(&buf, task->name);
        nt_jb_puts(&buf, ",\"project\":"); nt_jb_str(&buf, nt_model_project_name(model, task->project_id));
        nt_format_duration(nt_model_total_seconds(model, task->id), duration, sizeof(duration));
        nt_jb_puts(&buf, ",\"total_time\":"); nt_jb_int(&buf, nt_model_total_seconds(model, task->id));
        nt_jb_puts(&buf, ",\"duration\":"); nt_jb_str(&buf, duration);
        nt_jb_puts(&buf, ",\"is_timing\":"); nt_jb_bool(&buf, task->timing);
        nt_jb_puts(&buf, ",\"is_hidden\":"); nt_jb_bool(&buf, task->hidden);
        nt_jb_puts(&buf, ",\"tags\":[");
        {
            char tags[NT_TAGS_LEN];
            char *part;
            int first_tag = 1;
            snprintf(tags, sizeof(tags), "%s", task->tags);
            part = strtok(tags, ",");
            while (part != NULL)
            {
                char name[64];
                snprintf(name, sizeof(name), "%s", part);
                nt_str_trim(name);
                if (name[0] != '\0')
                {
                    nt_jb_puts(&buf, first_tag ? "" : ",");
                    nt_jb_str(&buf, name);
                    first_tag = 0;
                }
                part = strtok(NULL, ",");
            }
        }
        nt_jb_puts(&buf, "],\"annotations\":[");
        {
            NtAnnotation *annotations[256];
            size_t count = nt_model_task_annotations(model, task->id, annotations, 256);
            for (j = 0; j < count; j += 1)
            {
                char when[32];
                nt_format_datetime(annotations[j]->created_at, when, sizeof(when));
                nt_jb_puts(&buf, j == 0 ? "{\"date\":" : ",{\"date\":");
                nt_jb_str(&buf, when);
                nt_jb_puts(&buf, ",\"text\":"); nt_jb_str(&buf, annotations[j]->text);
                nt_jb_puts(&buf, "}");
            }
        }
        nt_jb_puts(&buf, "]}");
    }
    nt_jb_puts(&buf, "\n  ]\n}\n");
    file = fopen(path, "wb");
    if (file != NULL)
    {
        result = fwrite(buf.data, 1, buf.len, file) == buf.len ? 0 : -1;
        fclose(file);
    }
    nt_jb_free(&buf);
    return result;
}

int nt_model_export_csv(const NtModel *model, const char *path)
{
    FILE *file = fopen(path, "wb");
    size_t i;
    char today[NT_DATE_LEN];

    if (file == NULL)
        return -1;
    nt_today(today);
    fprintf(file, "task_id,task_name,project,tags,is_timing,is_hidden,total_seconds,today_seconds\n");
    for (i = 0; i < model->ntasks; i += 1)
    {
        const NtTask *task = &model->tasks[i];
        fprintf(file, "%d,\"%s\",\"%s\",\"%s\",%d,%d,%lld,%lld\n",
                task->id, task->name, nt_model_project_name(model, task->project_id), task->tags,
                task->timing, task->hidden,
                (long long)nt_model_total_seconds(model, task->id),
                (long long)nt_model_day_seconds(model, task->id, today));
    }
    fclose(file);
    return 0;
}

static int i_split(char *line, char **fields, int max)
{
    int count = 0;
    char *p = line;

    while (count < max)
    {
        fields[count++] = p;
        while (*p != '\0' && *p != ',')
        {
            if (*p == '"')
            {
                char *dst = p;
                p += 1;
                while (*p != '\0' && *p != '"')
                    *dst++ = *p++;
                if (*p == '"')
                    p += 1;
                *dst = '\0';
                continue;
            }
            p += 1;
        }
        if (*p == '\0')
            break;
        *p++ = '\0';
    }
    return count;
}

int nt_model_import_csv(NtModel *model, const char *path, char *error, size_t error_size)
{
    FILE *file = fopen(path, "rb");
    char line[4096];
    int imported = 0;

    if (file == NULL)
    {
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "Cannot open CSV file");
        return -1;
    }
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *fields[8];
        int count;
        int project_id = -1;
        char *name;
        char *project;
        char *tags;
        NtTask *task;

        nt_str_trim(line);
        if (line[0] == '\0')
            continue;
        count = i_split(line, fields, 8);
        if (count < 2)
            continue;
        name = fields[count >= 3 ? 1 : 0];
        project = count >= 3 ? fields[2] : "";
        tags = count >= 4 ? fields[3] : "";
        if (strcmp(name, "task_name") == 0 || strcmp(name, "name") == 0)
            continue;
        nt_str_trim(name);
        nt_str_trim(project);
        nt_str_trim(tags);
        if (name[0] == '\0')
            continue;
        if (project[0] != '\0')
        {
            NtProject *existing = NULL;
            size_t i;
            for (i = 0; i < model->nprojects; i += 1)
            {
                if (strcmp(model->projects[i].name, project) == 0)
                {
                    existing = &model->projects[i];
                    break;
                }
            }
            project_id = existing != NULL ? existing->id : nt_model_add_project(model, project)->id;
        }
        task = nt_model_add_task(model, name, project_id, tags);
        if (task != NULL && count >= 8)
        {
            char today[NT_DATE_LEN];
            int64_t seconds = strtoll(fields[count - 2], NULL, 10);
            nt_today(today);
            if (seconds > 0)
                nt_model_add_time(model, task->id, today, seconds);
        }
        imported += 1;
    }
    fclose(file);
    return imported;
}
