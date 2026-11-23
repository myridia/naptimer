#include "timer.h"

#include <stdlib.h>

static void i_set_timing(NtModel *model, NtTask *task, int timing, time_t last_start)
{
    task->timing = timing;
    task->last_start = last_start;
    task->paused_at = 0;
    task->paused_secs = 0;
    nt_model_save_task(model, task);
}

void nt_timer_start(NtModel *model, int task_id, int stop_others)
{
    NtTask *task;
    time_t now = time(NULL);
    size_t i;

    if (stop_others)
    {
        for (i = 0; i < model->ntasks; i += 1)
        {
            if (model->tasks[i].id != task_id && model->tasks[i].timing)
                nt_timer_stop(model, model->tasks[i].id);
        }
    }
    task = nt_model_task(model, task_id);
    if (task == NULL)
        return;
    if (task->timing)
        nt_timer_commit(model, task, now);
    i_set_timing(model, task, 1, now);
}

void nt_timer_stop(NtModel *model, int task_id)
{
    NtTask *task = nt_model_task(model, task_id);
    time_t now = time(NULL);

    if (task == NULL || !task->timing)
        return;
    nt_timer_commit(model, task, now);
    i_set_timing(model, task, 0, 0);
}

void nt_timer_stop_all(NtModel *model)
{
    size_t i;
    for (i = 0; i < model->ntasks; i += 1)
    {
        if (model->tasks[i].timing)
            nt_timer_stop(model, model->tasks[i].id);
    }
}

void nt_timer_toggle(NtModel *model, int task_id)
{
    NtTask *task = nt_model_task(model, task_id);
    if (task == NULL)
        return;
    if (task->timing)
        nt_timer_stop(model, task_id);
    else
        nt_timer_start(model, task_id, 1);
}

int nt_timer_running_count(const NtModel *model)
{
    size_t i;
    int count = 0;
    for (i = 0; i < model->ntasks; i += 1)
    {
        if (model->tasks[i].timing && !model->tasks[i].hidden)
            count += 1;
    }
    return count;
}

int64_t nt_timer_elapsed(const NtModel *model, const NtTask *task, time_t now)
{
    int64_t elapsed;

    if (task == NULL || !task->timing || task->last_start <= 0)
        return 0;
    elapsed = (int64_t)now - (int64_t)task->last_start - task->paused_secs;
    if (task->paused_at > 0)
        elapsed -= (int64_t)now - (int64_t)task->paused_at;
    return elapsed > 0 ? elapsed : 0;
}

void nt_timer_commit(NtModel *model, NtTask *task, time_t now)
{
    int64_t seconds;
    time_t from;

    if (task == NULL || !task->timing || task->last_start <= 0)
        return;
    seconds = nt_timer_elapsed(model, task, now);
    if (seconds > 0)
    {
        from = (time_t)((int64_t)now - seconds);
        nt_model_flush_elapsed(model, task->id, from, now);
    }
    task->last_start = now;
    task->paused_at = 0;
    task->paused_secs = 0;
    nt_model_save_task(model, task);
}

void nt_timer_autosave(NtModel *model, time_t now)
{
    size_t i;
    if (!model->settings.auto_save)
        return;
    for (i = 0; i < model->ntasks; i += 1)
    {
        if (model->tasks[i].timing)
            nt_timer_commit(model, &model->tasks[i], now);
    }
}

void nt_timer_pause_all(NtModel *model, time_t now)
{
    size_t i;
    int any = 0;
    for (i = 0; i < model->ntasks; i += 1)
    {
        if (model->tasks[i].timing && model->tasks[i].paused_at == 0)
        {
            model->tasks[i].paused_at = now;
            any = 1;
        }
    }
    if (any)
    {
        model->session.idle_since = now;
        model->session.idle_pending = 1;
    }
}

void nt_timer_resume_all(NtModel *model, time_t now, int revert)
{
    size_t i;
    time_t idle = 0;
    char today[NT_DATE_LEN];

    if (model->session.idle_since > 0)
        idle = now - model->session.idle_since;
    if (idle < 0)
        idle = 0;
    nt_today(today);
    for (i = 0; i < model->ntasks; i += 1)
    {
        NtTask *task = &model->tasks[i];
        if (task->paused_at > 0)
        {
            time_t paused = now - task->paused_at;
            if (paused < 0)
                paused = 0;
            task->paused_secs += (int64_t)paused;
            task->paused_at = 0;
            if (revert && paused > 0)
                nt_model_add_time(model, task->id, today, -(int64_t)paused);
            nt_model_save_task(model, task);
        }
    }
    model->session.idle_since = 0;
    model->session.idle_pending = 0;
    nt_model_save_session(model);
}
