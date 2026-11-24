#ifndef NAPTIMER_TIMER_H
#define NAPTIMER_TIMER_H

#include "model.h"

void nt_timer_start(NtModel *model, int task_id, int stop_others);
void nt_timer_stop(NtModel *model, int task_id);
void nt_timer_stop_all(NtModel *model);
void nt_timer_toggle(NtModel *model, int task_id);
int nt_timer_running_count(const NtModel *model);
int64_t nt_timer_elapsed(const NtModel *model, const NtTask *task, time_t now);
void nt_timer_commit(NtModel *model, NtTask *task, time_t now);
void nt_timer_autosave(NtModel *model, time_t now);
void nt_timer_pause_all(NtModel *model, time_t now);
void nt_timer_resume_all(NtModel *model, time_t now, int revert);

#endif
