#include "gui.h"

#include "idle.h"
#include "model.h"
#include "report.h"
#include "store.h"
#include "timer.h"
#include "util.h"

#include <nappgui.h>
#include <sewer/bstd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct _App App;
typedef struct _Prompt Prompt;

struct _Prompt
{
    Window *window;
    Edit *edit;
    PopUp *popup;
    App *app;
    bool_t accepted;
};

struct _App
{
    Window *window;
    Panel *panel;
    TableView *table;
    Edit *filter;
    Edit *quick;
    Label *status;
    Label *date_label;
    Label *total_label;
    Button *start_button;
    Button *stop_all_button;
    Layout *quick_layout;

    NtStore store;
    NtModel model;
    char db_path[NT_PATH_LEN];

    int *rows;
    size_t nrows;

    char search[256];
    char cell[256];
    int show_quick;
    int64_t time_buffer;

    int undo_kind;
    int undo_task_id;
    int64_t undo_seconds;
    int undo_task_ids[512];
    size_t n_undo_task_ids;

    time_t last_tick;
    time_t last_reminder;
    time_t idle_since;
    int idle_prompt_open;
};

enum
{
    UNDO_NONE,
    UNDO_HIDE,
    UNDO_STOP_ALL,
    UNDO_ADJUST
};

static void i_set_status(App *app, const char_t *text)
{
    label_text(app->status, text != NULL ? text : "");
}

static bool_t i_task_visible(const App *app, const NtTask *task)
{
    if (task->hidden)
        return FALSE;
    if (app->search[0] == '\0')
        return TRUE;
    if (nt_contains_ci(task->name, app->search))
        return TRUE;
    if (nt_contains_ci(task->tags, app->search))
        return TRUE;
    if (nt_contains_ci(nt_model_project_name(&app->model, task->project_id), app->search))
        return TRUE;
    return FALSE;
}

static void i_rebuild_rows(App *app)
{
    size_t i;
    size_t count = 0;

    if (app->rows != NULL)
        free(app->rows);
    app->rows = (int *)malloc(sizeof(int) * (app->model.ntasks > 0 ? app->model.ntasks : 1));
    for (i = 0; i < app->model.ntasks; i += 1)
    {
        if (i_task_visible(app, &app->model.tasks[i]))
            app->rows[count++] = app->model.tasks[i].id;
    }
    app->nrows = count;
}

static NtTask *i_selected_task(App *app)
{
    const ArrSt(uint32_t) *selected = tableview_selected(app->table);
    uint32_t row;
    if (arrst_size(selected, uint32_t) == 0)
        return NULL;
    row = *arrst_get_const(selected, 0, uint32_t);
    if ((size_t)row >= app->nrows)
        return NULL;
    return nt_model_task(&app->model, app->rows[row]);
}

static void i_refresh(App *app)
{
    char date[NT_DATE_LEN];
    char total[64];
    int64_t sum = 0;
    size_t i;

    i_rebuild_rows(app);
    tableview_update(app->table);

    if (app->model.session.view_date[0] == '\0')
        nt_today(date);
    else
        snprintf(date, sizeof(date), "%s", app->model.session.view_date);
    nt_format_date_long(date, app->cell, sizeof(app->cell));
    label_text(app->date_label, app->cell);

    for (i = 0; i < app->model.ntasks; i += 1)
        sum += nt_model_day_seconds(&app->model, app->model.tasks[i].id, date);
    bstd_sprintf(total, sizeof(total), "%s: %02lld:%02lld:%02lld",
                 app->model.session.view_date[0] == '\0' ? "Today" : "Day",
                 (long long)(sum / 3600), (long long)((sum % 3600) / 60), (long long)(sum % 60));
    label_text(app->total_label, total);
}

static void i_OnTableData(App *app, Event *e)
{
    switch (event_type(e))
    {
    case ekGUI_EVENT_TBL_NROWS:
        *event_result(e, uint32_t) = (uint32_t)app->nrows;
        break;
    case ekGUI_EVENT_TBL_CELL:
    {
        const EvTbPos *pos = event_params(e, EvTbPos);
        EvTbCell *cell = event_result(e, EvTbCell);
        NtTask *task;
        char duration[32];
        char date[NT_DATE_LEN];

        cell->text = "";
        cell->align = ekLEFT;
        if ((size_t)pos->row >= app->nrows)
            break;
        task = nt_model_task(&app->model, app->rows[pos->row]);
        if (task == NULL)
            break;
        if (app->model.session.view_date[0] == '\0')
            nt_today(date);
        else
            snprintf(date, sizeof(date), "%s", app->model.session.view_date);

        switch (pos->col)
        {
        case 0:
            bstd_sprintf(app->cell, sizeof(app->cell), "%s%s",
                         task->timing ? "● " : "",
                         nt_model_project_name(&app->model, task->project_id));
            cell->text = app->cell;
            break;
        case 1:
            cell->text = task->name;
            break;
        case 2:
            cell->text = task->tags;
            break;
        case 3:
            nt_format_duration(nt_model_day_seconds(&app->model, task->id, date), duration, sizeof(duration));
            bstd_sprintf(app->cell, sizeof(app->cell), "%s", duration);
            cell->text = app->cell;
            cell->align = ekCENTER;
            break;
        case 4:
            nt_format_duration(nt_model_total_seconds(&app->model, task->id), duration, sizeof(duration));
            bstd_sprintf(app->cell, sizeof(app->cell), "%s", duration);
            cell->text = app->cell;
            cell->align = ekCENTER;
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void i_OnFilterChange(App *app, Event *e)
{
    const char_t *text = edit_get_text(app->filter);
    bstd_sprintf(app->search, sizeof(app->search), "%s", text != NULL ? text : "");
    i_refresh(app);
    unref(e);
}

static void i_update_status_running(App *app)
{
    char text[256];
    int count = nt_timer_running_count(&app->model);
    if (count == 0)
        bstd_sprintf(text, sizeof(text), "No tasks timing. Select a task and press Start.");
    else
        bstd_sprintf(text, sizeof(text), "%d task(s) timing.", count);
    i_set_status(app, text);
}

static void i_OnStartStop(App *app, Event *e)
{
    NtTask *task = i_selected_task(app);
    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        unref(e);
        return;
    }
    nt_timer_toggle(&app->model, task->id);
    i_refresh(app);
    i_update_status_running(app);
    unref(e);
}

static void i_OnStopAll(App *app, Event *e)
{
    size_t i;
    app->undo_kind = UNDO_STOP_ALL;
    app->n_undo_task_ids = 0;
    for (i = 0; i < app->model.ntasks; i += 1)
    {
        if (app->model.tasks[i].timing && app->n_undo_task_ids < 512)
            app->undo_task_ids[app->n_undo_task_ids++] = app->model.tasks[i].id;
    }
    nt_timer_stop_all(&app->model);
    i_refresh(app);
    i_set_status(app, "All timers stopped. (Undo available)");
    unref(e);
}

static void i_prompt_close(Prompt *prompt)
{
    window_stop_modal(prompt->window, prompt->accepted ? 1 : 0);
}

static void i_OnPromptOk(Prompt *prompt, Event *e)
{
    prompt->accepted = TRUE;
    i_prompt_close(prompt);
    unref(e);
}

static void i_OnPromptCancel(Prompt *prompt, Event *e)
{
    prompt->accepted = FALSE;
    i_prompt_close(prompt);
    unref(e);
}

static bool_t i_prompt_text(App *app, const char_t *title, const char_t *prompt_label, const char_t *initial, char *out, size_t out_size)
{
    Window *window = window_create(ekWINDOW_TITLE | ekWINDOW_CLOSE | ekWINDOW_ESC);
    Layout *layout = layout_create(1, 3);
    Layout *buttons = layout_create(3, 1);
    Label *label = label_create();
    Edit *edit = edit_create();
    Button *ok = button_push();
    Button *cancel = button_push();
    Panel *panel = panel_create();
    Prompt prompt;
    bool_t result = FALSE;

    prompt.window = window;
    prompt.edit = edit;
    prompt.popup = NULL;
    prompt.app = app;
    prompt.accepted = FALSE;

    window_title(window, title);
    label_text(label, prompt_label);
    edit_text(edit, initial != NULL ? initial : "");
    button_text(ok, "OK");
    button_text(cancel, "Cancel");
    button_OnClick(ok, listener(&prompt, i_OnPromptOk, Prompt));
    button_OnClick(cancel, listener(&prompt, i_OnPromptCancel, Prompt));

    layout_label(layout, label, 0, 0);
    layout_edit(layout, edit, 0, 1);
    layout_layout(layout, buttons, 0, 2);
    layout_button(buttons, ok, 1, 0);
    layout_button(buttons, cancel, 2, 0);
    layout_hexpand(buttons, 0);
    layout_hmargin(buttons, 1, 6);
    layout_vmargin(layout, 0, 8);
    layout_vmargin(layout, 1, 8);
    layout_margin(layout, 12);
    panel_layout(panel, layout);
    window_panel(window, panel);
    window_defbutton(window, ok);
    window_focus(window, guicontrol(edit));
    window_modal(window, app->window);

    if (prompt.accepted == TRUE)
    {
        const char_t *text = edit_get_text(edit);
        bstd_sprintf(out, (uint32_t)out_size, "%s", text != NULL ? text : "");
        result = TRUE;
    }
    window_destroy(&window);
    return result;
}

static int i_prompt_project(App *app, int current_project, const char_t *title)
{
    Window *window = window_create(ekWINDOW_TITLE | ekWINDOW_CLOSE | ekWINDOW_ESC);
    Layout *layout = layout_create(1, 3);
    Layout *buttons = layout_create(3, 1);
    Label *label = label_create();
    PopUp *popup = popup_create();
    Button *ok = button_push();
    Button *cancel = button_push();
    Panel *panel = panel_create();
    Prompt prompt;
    size_t i;
    uint32_t selected = 0;

    prompt.window = window;
    prompt.edit = NULL;
    prompt.popup = popup;
    prompt.app = app;
    prompt.accepted = FALSE;

    popup_add_elem(popup, "None", NULL);
    for (i = 0; i < app->model.nprojects; i += 1)
    {
        popup_add_elem(popup, app->model.projects[i].name, NULL);
        if (app->model.projects[i].id == current_project)
            selected = (uint32_t)i + 1;
    }
    popup_selected(popup, selected);

    window_title(window, title);
    label_text(label, "Project:");
    button_text(ok, "OK");
    button_text(cancel, "Cancel");
    button_OnClick(ok, listener(&prompt, i_OnPromptOk, Prompt));
    button_OnClick(cancel, listener(&prompt, i_OnPromptCancel, Prompt));

    layout_label(layout, label, 0, 0);
    layout_popup(layout, popup, 0, 1);
    layout_layout(layout, buttons, 0, 2);
    layout_button(buttons, ok, 1, 0);
    layout_button(buttons, cancel, 2, 0);
    layout_hexpand(buttons, 0);
    layout_hmargin(buttons, 1, 6);
    layout_vmargin(layout, 0, 8);
    layout_vmargin(layout, 1, 8);
    layout_margin(layout, 12);
    panel_layout(panel, layout);
    window_panel(window, panel);
    window_defbutton(window, ok);
    window_modal(window, app->window);

    if (prompt.accepted == TRUE)
    {
        uint32_t idx = popup_get_selected(popup);
        window_destroy(&window);
        if (idx == 0)
            return -1;
        if (idx - 1 < app->model.nprojects)
            return app->model.projects[idx - 1].id;
        return -1;
    }
    window_destroy(&window);
    return current_project;
}

static void i_OnNewTask(App *app, Event *e)
{
    char name[NT_NAME_LEN];
    char tags[NT_TAGS_LEN];
    int project_id;

    name[0] = '\0';
    tags[0] = '\0';
    if (i_prompt_text(app, "New Task", "Task name:", "", name, sizeof(name)) == FALSE)
    {
        unref(e);
        return;
    }
    nt_str_trim(name);
    if (name[0] == '\0')
    {
        unref(e);
        return;
    }
    project_id = i_prompt_project(app, -1, "Task Project");
    if (i_prompt_text(app, "Task Tags", "Tags (comma-separated, optional):", "", tags, sizeof(tags)) == FALSE)
        tags[0] = '\0';
    nt_model_add_task(&app->model, name, project_id, tags);
    i_refresh(app);
    i_set_status(app, "Task added.");
    unref(e);
}

static void i_OnEditTask(App *app, Event *e)
{
    NtTask *task = i_selected_task(app);
    char name[NT_NAME_LEN];
    char tags[NT_TAGS_LEN];
    int project_id;

    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        unref(e);
        return;
    }
    bstd_sprintf(name, sizeof(name), "%s", task->name);
    bstd_sprintf(tags, sizeof(tags), "%s", task->tags);
    if (i_prompt_text(app, "Edit Task", "Task name:", name, name, sizeof(name)) == FALSE)
    {
        unref(e);
        return;
    }
    nt_str_trim(name);
    if (name[0] == '\0')
    {
        unref(e);
        return;
    }
    project_id = i_prompt_project(app, task->project_id, "Task Project");
    if (i_prompt_text(app, "Task Tags", "Tags (comma-separated):", tags, tags, sizeof(tags)) == FALSE)
    {
        unref(e);
        return;
    }
    nt_model_update_task(&app->model, task->id, name, project_id, tags);
    i_refresh(app);
    i_set_status(app, "Task updated.");
    unref(e);
}

static void i_OnDeleteTask(App *app, Event *e)
{
    NtTask *task = i_selected_task(app);
    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        unref(e);
        return;
    }
    nt_model_delete_task(&app->model, task->id);
    i_refresh(app);
    i_set_status(app, "Task deleted.");
    unref(e);
}

static void i_OnHideTask(App *app, Event *e)
{
    NtTask *task = i_selected_task(app);
    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        unref(e);
        return;
    }
    app->undo_kind = UNDO_HIDE;
    app->undo_task_id = task->id;
    nt_model_hide_task(&app->model, task->id, 1);
    i_refresh(app);
    i_set_status(app, "Task hidden. (Undo available)");
    unref(e);
}

static void i_OnUnhideTasks(App *app, Event *e)
{
    size_t i;
    int count = 0;
    for (i = 0; i < app->model.ntasks; i += 1)
    {
        if (app->model.tasks[i].hidden)
        {
            nt_model_hide_task(&app->model, app->model.tasks[i].id, 0);
            count += 1;
        }
    }
    i_refresh(app);
    if (count == 0)
        i_set_status(app, "No hidden tasks.");
    else
        i_set_status(app, "Hidden tasks restored.");
    unref(e);
}

static void i_OnNewProject(App *app, Event *e)
{
    char name[NT_NAME_LEN];
    name[0] = '\0';
    if (i_prompt_text(app, "New Project", "Project name:", "", name, sizeof(name)) == FALSE)
    {
        unref(e);
        return;
    }
    nt_str_trim(name);
    if (name[0] != '\0')
    {
        nt_model_add_project(&app->model, name);
        i_refresh(app);
        i_set_status(app, "Project added.");
    }
    unref(e);
}

static void i_OnEditProject(App *app, Event *e)
{
    int project_id;
    NtProject *project;
    char name[NT_NAME_LEN];

    if (app->model.nprojects == 0)
    {
        i_set_status(app, "No projects to edit.");
        unref(e);
        return;
    }
    project_id = i_prompt_project(app, app->model.projects[0].id, "Select Project");
    project = nt_model_project(&app->model, project_id);
    if (project == NULL)
    {
        unref(e);
        return;
    }
    bstd_sprintf(name, sizeof(name), "%s", project->name);
    if (i_prompt_text(app, "Edit Project", "Project name:", name, name, sizeof(name)) == FALSE)
    {
        unref(e);
        return;
    }
    nt_str_trim(name);
    if (name[0] != '\0')
    {
        nt_model_rename_project(&app->model, project_id, name);
        i_refresh(app);
        i_set_status(app, "Project updated.");
    }
    unref(e);
}

static void i_OnAnnotate(App *app, Event *e)
{
    NtTask *task = i_selected_task(app);
    char text[NT_TEXT_LEN];
    text[0] = '\0';
    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        unref(e);
        return;
    }
    if (i_prompt_text(app, "Add Annotation", "Annotation:", "", text, sizeof(text)) == FALSE)
    {
        unref(e);
        return;
    }
    nt_str_trim(text);
    if (text[0] != '\0')
    {
        nt_model_add_annotation(&app->model, task->id, text);
        i_set_status(app, "Annotation added.");
    }
    unref(e);
}

static void i_add_seconds(App *app, int64_t seconds)
{
    NtTask *task = i_selected_task(app);
    char date[NT_DATE_LEN];
    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        return;
    }
    if (app->model.session.view_date[0] == '\0')
        nt_today(date);
    else
        snprintf(date, sizeof(date), "%s", app->model.session.view_date);
    app->undo_kind = UNDO_ADJUST;
    app->undo_task_id = task->id;
    app->undo_seconds = seconds;
    nt_model_add_time(&app->model, task->id, date, seconds);
    i_refresh(app);
}

static void i_OnAdjust(App *app, Event *e)
{
    const EvButton *p = event_params(e, EvButton);
    int64_t seconds = (int64_t)p->index;
    i_add_seconds(app, seconds);
    i_set_status(app, "Time adjusted. (Undo available)");
    unref(e);
}

static void i_OnSetZero(App *app, Event *e)
{
    NtTask *task = i_selected_task(app);
    char date[NT_DATE_LEN];
    int64_t current;
    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        unref(e);
        return;
    }
    if (app->model.session.view_date[0] == '\0')
        nt_today(date);
    else
        snprintf(date, sizeof(date), "%s", app->model.session.view_date);
    current = nt_model_day_seconds(&app->model, task->id, date);
    app->time_buffer = current;
    nt_model_set_time(&app->model, task->id, date, 0);
    i_refresh(app);
    i_set_status(app, "Time set to zero and copied to buffer.");
    unref(e);
}

static void i_OnCopyTime(App *app, Event *e)
{
    NtTask *task = i_selected_task(app);
    char date[NT_DATE_LEN];
    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        unref(e);
        return;
    }
    if (app->model.session.view_date[0] == '\0')
        nt_today(date);
    else
        snprintf(date, sizeof(date), "%s", app->model.session.view_date);
    app->time_buffer = nt_model_day_seconds(&app->model, task->id, date);
    i_set_status(app, "Time copied to buffer.");
    unref(e);
}

static void i_OnPasteTime(App *app, Event *e)
{
    NtTask *task = i_selected_task(app);
    char date[NT_DATE_LEN];
    if (task == NULL)
    {
        i_set_status(app, "Select a task first.");
        unref(e);
        return;
    }
    if (app->model.session.view_date[0] == '\0')
        nt_today(date);
    else
        snprintf(date, sizeof(date), "%s", app->model.session.view_date);
    nt_model_add_time(&app->model, task->id, date, app->time_buffer);
    i_refresh(app);
    i_set_status(app, "Time pasted from buffer.");
    unref(e);
}

static void i_OnUndo(App *app, Event *e)
{
    size_t i;
    char date[NT_DATE_LEN];

    if (app->undo_kind == UNDO_HIDE)
    {
        nt_model_hide_task(&app->model, app->undo_task_id, 0);
        i_set_status(app, "Task unhidden.");
    }
    else if (app->undo_kind == UNDO_STOP_ALL)
    {
        for (i = 0; i < app->n_undo_task_ids; i += 1)
            nt_timer_start(&app->model, app->undo_task_ids[i], 0);
        i_set_status(app, "Timers restarted.");
    }
    else if (app->undo_kind == UNDO_ADJUST)
    {
        if (app->model.session.view_date[0] == '\0')
            nt_today(date);
        else
            snprintf(date, sizeof(date), "%s", app->model.session.view_date);
        nt_model_add_time(&app->model, app->undo_task_id, date, -app->undo_seconds);
        i_set_status(app, "Time adjustment undone.");
    }
    else
    {
        i_set_status(app, "Nothing to undo.");
        unref(e);
        return;
    }
    app->undo_kind = UNDO_NONE;
    i_refresh(app);
    unref(e);
}

static void i_OnPrevDay(App *app, Event *e)
{
    char date[NT_DATE_LEN];
    if (app->model.session.view_date[0] == '\0')
        nt_today(date);
    else
        snprintf(date, sizeof(date), "%s", app->model.session.view_date);
    nt_date_shift(date, -1, app->model.session.view_date);
    nt_model_save_session(&app->model);
    i_refresh(app);
    unref(e);
}

static void i_OnNextDay(App *app, Event *e)
{
    char date[NT_DATE_LEN];
    char today[NT_DATE_LEN];
    nt_today(today);
    if (app->model.session.view_date[0] == '\0')
    {
        unref(e);
        return;
    }
    nt_date_shift(app->model.session.view_date, 1, date);
    if (nt_date_cmp(date, today) >= 0)
        app->model.session.view_date[0] = '\0';
    else
        snprintf(app->model.session.view_date, NT_DATE_LEN, "%s", date);
    nt_model_save_session(&app->model);
    i_refresh(app);
    unref(e);
}

static void i_OnToday(App *app, Event *e)
{
    app->model.session.view_date[0] = '\0';
    nt_model_save_session(&app->model);
    i_refresh(app);
    unref(e);
}

static void i_OnQuickEntry(App *app, Event *e)
{
    window_focus(app->window, guicontrol(app->quick));
    unref(e);
}

static void i_parse_quick(App *app, const char *input)
{
    char text[512];
    char *at;
    char *hash;
    char task_name[NT_NAME_LEN];
    char project_name[NT_NAME_LEN];
    char tags[NT_TAGS_LEN];
    int project_id = -1;
    NtTask *task = NULL;
    size_t i;

    bstd_sprintf(text, sizeof(text), "%s", input);
    nt_str_trim(text);
    if (text[0] == '\0')
        return;

    tags[0] = '\0';
    hash = strchr(text, '#');
    if (hash != NULL)
    {
        char *p = hash;
        bstd_sprintf(tags, sizeof(tags), "%s", hash + 1);
        *p = '\0';
    }

    project_name[0] = '\0';
    at = strchr(text, '@');
    if (at != NULL)
    {
        *at = '\0';
        bstd_sprintf(project_name, sizeof(project_name), "%s", at + 1);
        nt_str_trim(project_name);
    }
    bstd_sprintf(task_name, sizeof(task_name), "%s", text);
    nt_str_trim(task_name);
    if (task_name[0] == '\0')
        return;

    if (project_name[0] != '\0')
    {
        for (i = 0; i < app->model.nprojects; i += 1)
        {
            if (strcmp(app->model.projects[i].name, project_name) == 0)
            {
                project_id = app->model.projects[i].id;
                break;
            }
        }
        if (project_id < 0)
            project_id = nt_model_add_project(&app->model, project_name)->id;
    }

    for (i = 0; i < app->model.ntasks; i += 1)
    {
        if (strcmp(app->model.tasks[i].name, task_name) == 0 &&
            (project_id < 0 || app->model.tasks[i].project_id == project_id))
        {
            task = &app->model.tasks[i];
            break;
        }
    }
    if (task == NULL)
        task = nt_model_add_task(&app->model, task_name, project_id, tags);
    else if (tags[0] != '\0')
        nt_model_sync_tags(&app->model, task->id, tags);

    nt_timer_start(&app->model, task->id, 1);
    edit_text(app->quick, "");
    i_refresh(app);
    i_set_status(app, "Quick entry started.");
}

static void i_OnQuickAccept(App *app, Event *e)
{
    const char_t *text = edit_get_text(app->quick);
    i_parse_quick(app, text != NULL ? text : "");
    unref(e);
}

static void i_OnRowClick(App *app, Event *e)
{
    i_OnStartStop(app, e);
}

static void i_OnReport(App *app, Event *e)
{
    char from[NT_DATE_LEN];
    char to[NT_DATE_LEN];
    char *report = NULL;
    const char_t *types[] = {"txt", "html"};
    const char_t *file;
    NtReportFormat format = NT_REPORT_TEXT;

    nt_today(to);
    nt_date_shift(to, -6, from);
    file = comwin_save_file(app->window, types, 2, NULL);
    if (file == NULL)
    {
        unref(e);
        return;
    }
    if (strstr(file, ".html") != NULL)
        format = NT_REPORT_HTML;
    nt_report_generate(&app->model, NT_REPORT_DAILY, format, from, to, 0, NULL, &report);
    {
        FILE *f = fopen(file, "wb");
        if (f != NULL)
        {
            fputs(report, f);
            fclose(f);
            i_set_status(app, "Report saved.");
        }
        else
        {
            i_set_status(app, "Could not save report.");
        }
    }
    free(report);
    unref(e);
}

static void i_OnExportJson(App *app, Event *e)
{
    const char_t *types[] = {"json"};
    const char_t *file = comwin_save_file(app->window, types, 1, NULL);
    if (file != NULL)
    {
        if (nt_model_export_json(&app->model, file) == 0)
            i_set_status(app, "JSON export saved.");
        else
            i_set_status(app, "JSON export failed.");
    }
    unref(e);
}

static void i_OnExportCsv(App *app, Event *e)
{
    const char_t *types[] = {"csv"};
    const char_t *file = comwin_save_file(app->window, types, 1, NULL);
    if (file != NULL)
    {
        if (nt_model_export_csv(&app->model, file) == 0)
            i_set_status(app, "CSV export saved.");
        else
            i_set_status(app, "CSV export failed.");
    }
    unref(e);
}

static void i_OnImportCsv(App *app, Event *e)
{
    const char_t *types[] = {"csv"};
    const char_t *file = comwin_open_file(app->window, types, 1, NULL);
    if (file != NULL)
    {
        char error[256];
        int imported = nt_model_import_csv(&app->model, file, error, sizeof(error));
        if (imported < 0)
            i_set_status(app, error[0] != '\0' ? error : "CSV import failed.");
        else
        {
            i_refresh(app);
            i_set_status(app, "CSV imported.");
        }
    }
    unref(e);
}

static void i_OnBackup2(App *app, Event *e)
{
    const char_t *types[] = {"db"};
    const char_t *file = comwin_save_file(app->window, types, 1, NULL);
    if (file != NULL)
    {
        char error[256];
        error[0] = '\0';
        if (nt_store_backup(&app->store, file, error, sizeof(error)) == 0)
            i_set_status(app, "Backup saved.");
        else
            i_set_status(app, error[0] != '\0' ? error : "Backup failed.");
    }
    unref(e);
}

static void i_OnRestore(App *app, Event *e)
{
    const char_t *types[] = {"db"};
    const char_t *file = comwin_open_file(app->window, types, 1, NULL);
    if (file != NULL)
    {
        char error[256];
        error[0] = '\0';
        if (nt_store_restore(&app->store, file, error, sizeof(error)) == 0)
        {
            nt_model_cleanup(&app->model);
            nt_model_init(&app->model, &app->store);
            nt_model_load(&app->model, error, sizeof(error));
            i_refresh(app);
            i_set_status(app, "Database restored.");
        }
        else
        {
            i_set_status(app, error[0] != '\0' ? error : "Restore failed.");
        }
    }
    unref(e);
}

static void i_OnQuit(App *app, Event *e)
{
    nt_timer_autosave(&app->model, time(NULL));
    nt_model_save_session(&app->model);
    osapp_finish();
    unref(e);
}

static void i_OnAbout(App *app, Event *e)
{
    Window *window = window_create(ekWINDOW_TITLE | ekWINDOW_CLOSE | ekWINDOW_ESC);
    Layout *layout = layout_create(1, 2);
    Label *text = label_create();
    Button *ok = button_push();
    Panel *panel = panel_create();
    Prompt prompt;

    prompt.window = window;
    prompt.app = app;
    prompt.accepted = TRUE;
    prompt.edit = NULL;
    prompt.popup = NULL;

    window_title(window, "About Naptimer");
    label_text(text, "Naptimer\n\nGTK-free time tracker ported to NAppGUI.\nData is stored as CouchDB-ready JSON docs in SQLite.");
    label_multiline(text, TRUE);
    button_text(ok, "OK");
    button_OnClick(ok, listener(&prompt, i_OnPromptOk, Prompt));
    layout_label(layout, text, 0, 0);
    layout_button(layout, ok, 0, 1);
    layout_margin(layout, 12);
    layout_vmargin(layout, 0, 10);
    panel_layout(panel, layout);
    window_panel(window, panel);
    window_defbutton(window, ok);
    window_modal(window, app->window);
    window_destroy(&window);
    unref(e);
}

static void i_show_idle_prompt(App *app)
{
    Window *window;
    Layout *layout;
    Layout *buttons;
    Label *label;
    Button *revert;
    Button *resume;
    Panel *panel;
    Prompt prompt;
    char message[256];
    int64_t idle_secs = 0;

    if (app->idle_prompt_open)
        return;
    app->idle_prompt_open = 1;
    if (app->idle_since > 0)
        idle_secs = (int64_t)(time(NULL) - app->idle_since);
    bstd_sprintf(message, sizeof(message), "You have been idle for at least %lld minutes.", (long long)(idle_secs / 60));

    window = window_create(ekWINDOW_TITLE | ekWINDOW_CLOSE | ekWINDOW_ESC);
    layout = layout_create(1, 2);
    buttons = layout_create(3, 1);
    label = label_create();
    revert = button_push();
    resume = button_push();
    panel = panel_create();

    prompt.window = window;
    prompt.app = app;
    prompt.accepted = FALSE;
    prompt.edit = NULL;
    prompt.popup = NULL;

    window_title(window, "Idle Detected");
    label_text(label, message);
    label_multiline(label, TRUE);
    button_text(revert, "Revert");
    button_text(resume, "Resume");
    button_OnClick(revert, listener(&prompt, i_OnPromptCancel, Prompt));
    button_OnClick(resume, listener(&prompt, i_OnPromptOk, Prompt));

    layout_label(layout, label, 0, 0);
    layout_layout(layout, buttons, 0, 1);
    layout_button(buttons, revert, 1, 0);
    layout_button(buttons, resume, 2, 0);
    layout_hexpand(buttons, 0);
    layout_hmargin(buttons, 1, 6);
    layout_vmargin(layout, 0, 10);
    layout_margin(layout, 12);
    panel_layout(panel, layout);
    window_panel(window, panel);
    window_defbutton(window, resume);
    window_modal(window, app->window);
    if (prompt.accepted)
        nt_timer_resume_all(&app->model, time(NULL), 0);
    else
        nt_timer_resume_all(&app->model, time(NULL), 1);
    app->idle_prompt_open = 0;
    app->idle_since = 0;
    window_destroy(&window);
    i_refresh(app);
}

static void i_tick(App *app)
{
    time_t now = time(NULL);
    int64_t idle;
    size_t i;

    if (now == app->last_tick)
        return;
    app->last_tick = now;

    if (app->model.settings.auto_save)
        nt_timer_autosave(&app->model, now);

    if (app->model.settings.idle_enabled && nt_idle_available())
    {
        idle = nt_idle_seconds();
        if (idle >= (int64_t)app->model.settings.idle_threshold * 60 && nt_timer_running_count(&app->model) > 0)
        {
            if (app->idle_since == 0)
            {
                app->idle_since = now - (time_t)idle;
                nt_timer_pause_all(&app->model, now);
            }
            i_show_idle_prompt(app);
        }
    }

    if (app->model.settings.reminder && nt_timer_running_count(&app->model) == 0)
    {
        if (app->last_reminder == 0)
            app->last_reminder = now;
        if (now - app->last_reminder >= (time_t)app->model.settings.reminder_interval * 60)
        {
            i_set_status(app, "Reminder: no task is currently being timed.");
            app->last_reminder = now;
        }
    }
    else
    {
        app->last_reminder = now;
    }

    for (i = 0; i < app->nrows; i += 1)
    {
        NtTask *task = nt_model_task(&app->model, app->rows[i]);
        if (task != NULL && task->timing)
        {
            tableview_update(app->table);
            break;
        }
    }
}

static void i_OnWindowClose(App *app, Event *e)
{
    nt_timer_autosave(&app->model, time(NULL));
    nt_model_save_session(&app->model);
    osapp_finish();
    unref(e);
}

static App *i_app_create(const char *db_path)
{
    App *app = heap_new0(App);
    Layout *layout = layout_create(1, 5);
    Layout *toolbar = layout_create(7, 1);
    Layout *footer = layout_create(5, 1);
    Label *filter_label = label_create();
    Button *new_task = button_push();
    Button *start = button_push();
    Button *stop_all = button_push();
    Button *prev = button_push();
    Button *next = button_push();
    Button *today = button_push();
    Menu *menu;
    MenuItem *file_item;
    Menu *file_menu;

    bstd_sprintf(app->db_path, sizeof(app->db_path), "%s", db_path);
    if (nt_store_open(db_path, &app->store, NULL, 0) != 0)
    {
        heap_delete(&app, App);
        return NULL;
    }
    nt_model_init(&app->model, &app->store);
    nt_model_load(&app->model, NULL, 0);

    app->window = window_create(ekWINDOW_STDRES);
    window_title(app->window, "Naptimer");
    window_OnClose(app->window, listener(app, i_OnWindowClose, App));

    app->panel = panel_create();
    app->table = tableview_create();
    app->filter = edit_create();
    app->quick = edit_create();
    app->status = label_create();
    app->date_label = label_create();
    app->total_label = label_create();
    app->start_button = start;
    app->stop_all_button = stop_all;
    app->quick_layout = layout_create(2, 1);

    tableview_OnData(app->table, listener(app, i_OnTableData, App));
    tableview_OnRowClick(app->table, listener(app, i_OnRowClick, App));
    tableview_header_resizable(app->table, TRUE);
    tableview_row_height(app->table, 26);
    tableview_new_column_text(app->table);
    tableview_header_title(app->table, 0, "Project");
    tableview_column_width(app->table, 0, 160);
    tableview_new_column_text(app->table);
    tableview_header_title(app->table, 1, "Task");
    tableview_column_width(app->table, 1, 260);
    tableview_new_column_text(app->table);
    tableview_header_title(app->table, 2, "Tags");
    tableview_column_width(app->table, 2, 180);
    tableview_new_column_text(app->table);
    tableview_header_title(app->table, 3, "Today");
    tableview_column_width(app->table, 3, 110);
    tableview_header_align(app->table, 3, ekCENTER);
    tableview_new_column_text(app->table);
    tableview_header_title(app->table, 4, "Total");
    tableview_column_width(app->table, 4, 110);
    tableview_header_align(app->table, 4, ekCENTER);

    label_text(filter_label, "Search:");
    edit_OnChange(app->filter, listener(app, i_OnFilterChange, App));
    button_text(new_task, "New Task");
    button_text(start, "Start/Stop");
    button_text(stop_all, "Stop All");
    button_text(prev, "<");
    button_text(next, ">");
    button_text(today, "Today");

    button_OnClick(new_task, listener(app, i_OnNewTask, App));
    button_OnClick(start, listener(app, i_OnStartStop, App));
    button_OnClick(stop_all, listener(app, i_OnStopAll, App));
    button_OnClick(prev, listener(app, i_OnPrevDay, App));
    button_OnClick(next, listener(app, i_OnNextDay, App));
    button_OnClick(today, listener(app, i_OnToday, App));

    layout_button(toolbar, new_task, 0, 0);
    layout_button(toolbar, start, 1, 0);
    layout_button(toolbar, stop_all, 2, 0);
    layout_label(toolbar, filter_label, 3, 0);
    layout_edit(toolbar, app->filter, 4, 0);
    layout_hexpand(toolbar, 4);
    layout_hmargin(toolbar, 0, 6);
    layout_hmargin(toolbar, 1, 6);
    layout_hmargin(toolbar, 2, 10);
    layout_hmargin(toolbar, 3, 6);

    {
        Button *quick_go = button_push();
        button_text(quick_go, "Start");
        button_OnClick(quick_go, listener(app, i_OnQuickAccept, App));
        layout_edit(app->quick_layout, app->quick, 0, 0);
        layout_button(app->quick_layout, quick_go, 1, 0);
        layout_hexpand(app->quick_layout, 0);
        layout_hmargin(app->quick_layout, 0, 6);
        layout_margin(app->quick_layout, 0);
    }

    layout_button(footer, prev, 0, 0);
    layout_label(footer, app->date_label, 1, 0);
    layout_button(footer, next, 2, 0);
    layout_button(footer, today, 3, 0);
    layout_label(footer, app->total_label, 4, 0);
    layout_hexpand(footer, 4);
    layout_hmargin(footer, 0, 6);
    layout_hmargin(footer, 1, 6);
    layout_hmargin(footer, 2, 6);
    layout_hmargin(footer, 3, 10);
    layout_halign(footer, 4, 0, ekRIGHT);

    layout_layout(layout, toolbar, 0, 0);
    layout_layout(layout, app->quick_layout, 0, 1);
    layout_tableview(layout, app->table, 0, 2);
    layout_layout(layout, footer, 0, 3);
    layout_label(layout, app->status, 0, 4);
    layout_hexpand(layout, 0);
    layout_vexpand(layout, 2);
    layout_hsize(layout, 0, 940);
    layout_vsize(layout, 2, 460);
    layout_margin(layout, 10);
    layout_vmargin(layout, 0, 6);
    layout_vmargin(layout, 1, 6);
    layout_vmargin(layout, 3, 6);

    panel_layout(app->panel, layout);
    window_panel(app->window, app->panel);

    window_hotkey(app->window, ekKEY_N, ekMKEY_CONTROL, listener(app, i_OnNewTask, App));
    window_hotkey(app->window, ekKEY_E, ekMKEY_CONTROL, listener(app, i_OnEditTask, App));
    window_hotkey(app->window, ekKEY_S, ekMKEY_ALT, listener(app, i_OnStartStop, App));
    window_hotkey(app->window, ekKEY_T, ekMKEY_ALT, listener(app, i_OnStopAll, App));
    window_hotkey(app->window, ekKEY_Z, ekMKEY_CONTROL, listener(app, i_OnUndo, App));
    window_hotkey(app->window, ekKEY_R, ekMKEY_CONTROL, listener(app, i_OnReport, App));
    window_hotkey(app->window, ekKEY_LEFT, ekMKEY_ALT, listener(app, i_OnPrevDay, App));
    window_hotkey(app->window, ekKEY_RIGHT, ekMKEY_ALT, listener(app, i_OnNextDay, App));
    window_hotkey(app->window, ekKEY_HOME, ekMKEY_ALT, listener(app, i_OnToday, App));
    window_hotkey(app->window, ekKEY_SPACE, ekMKEY_CONTROL, listener(app, i_OnQuickEntry, App));
    window_hotkey(app->window, ekKEY_SUPR, ekMKEY_NONE, listener(app, i_OnDeleteTask, App));

    menu = menu_create();
    file_item = menuitem_create();
    menuitem_text(file_item, "File");
    file_menu = menu_create();
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "New Task");
        menuitem_OnClick(mi, listener(app, i_OnNewTask, App));
        menu_add_item(file_menu, mi);
    }
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "New Project");
        menuitem_OnClick(mi, listener(app, i_OnNewProject, App));
        menu_add_item(file_menu, mi);
    }
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "Report...");
        menuitem_OnClick(mi, listener(app, i_OnReport, App));
        menu_add_item(file_menu, mi);
    }
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "Export JSON...");
        menuitem_OnClick(mi, listener(app, i_OnExportJson, App));
        menu_add_item(file_menu, mi);
    }
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "Export CSV...");
        menuitem_OnClick(mi, listener(app, i_OnExportCsv, App));
        menu_add_item(file_menu, mi);
    }
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "Import CSV...");
        menuitem_OnClick(mi, listener(app, i_OnImportCsv, App));
        menu_add_item(file_menu, mi);
    }
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "Backup...");
        menuitem_OnClick(mi, listener(app, i_OnBackup2, App));
        menu_add_item(file_menu, mi);
    }
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "Restore...");
        menuitem_OnClick(mi, listener(app, i_OnRestore, App));
        menu_add_item(file_menu, mi);
    }
    menu_add_item(file_menu, menuitem_separator());
    {
        MenuItem *mi = menuitem_create();
        menuitem_text(mi, "Quit");
        menuitem_OnClick(mi, listener(app, i_OnQuit, App));
        menu_add_item(file_menu, mi);
    }
    menuitem_submenu(file_item, &file_menu);
    menu_add_item(menu, file_item);

    {
        MenuItem *edit_item = menuitem_create();
        Menu *edit_menu = menu_create();
        menuitem_text(edit_item, "Edit");
        {
            MenuItem *mi = menuitem_create();
            menuitem_text(mi, "Edit Task");
            menuitem_OnClick(mi, listener(app, i_OnEditTask, App));
            menu_add_item(edit_menu, mi);
        }
        {
            MenuItem *mi = menuitem_create();
            menuitem_text(mi, "Hide Task");
            menuitem_OnClick(mi, listener(app, i_OnHideTask, App));
            menu_add_item(edit_menu, mi);
        }
        {
            MenuItem *mi = menuitem_create();
            menuitem_text(mi, "Unhide Tasks");
            menuitem_OnClick(mi, listener(app, i_OnUnhideTasks, App));
            menu_add_item(edit_menu, mi);
        }
        {
            MenuItem *mi = menuitem_create();
            menuitem_text(mi, "Add Annotation");
            menuitem_OnClick(mi, listener(app, i_OnAnnotate, App));
            menu_add_item(edit_menu, mi);
        }
        {
            MenuItem *mi = menuitem_create();
            menuitem_text(mi, "Undo");
            menuitem_OnClick(mi, listener(app, i_OnUndo, App));
            menu_add_item(edit_menu, mi);
        }
        menuitem_submenu(edit_item, &edit_menu);
        menu_add_item(menu, edit_item);
    }

    {
        MenuItem *help_item = menuitem_create();
        Menu *help_menu = menu_create();
        menuitem_text(help_item, "Help");
        {
            MenuItem *mi = menuitem_create();
            menuitem_text(mi, "About Naptimer");
            menuitem_OnClick(mi, listener(app, i_OnAbout, App));
            menu_add_item(help_menu, mi);
        }
        menuitem_submenu(help_item, &help_menu);
        menu_add_item(menu, help_item);
    }

    osapp_menubar(menu, app->window);

    i_refresh(app);
    i_update_status_running(app);
    window_show(app->window);
    return app;
}

static void i_app_destroy(App **app)
{
    if (*app == NULL)
        return;
    nt_timer_autosave(&(*app)->model, time(NULL));
    nt_model_save_session(&(*app)->model);
    nt_model_cleanup(&(*app)->model);
    nt_store_close(&(*app)->store);
    if ((*app)->rows != NULL)
        free((*app)->rows);
    window_destroy(&(*app)->window);
    heap_delete(app, App);
}

App *nt_gui_create(const char *db_path)
{
    return i_app_create(db_path);
}

void nt_gui_destroy(App **app)
{
    i_app_destroy(app);
}

void nt_gui_update(App *app, const real64_t prtime, const real64_t ctime)
{
    unref(prtime);
    unref(ctime);
    if (app != NULL)
        i_tick(app);
}
