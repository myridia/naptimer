#ifndef NAPTIMER_GUI_H
#define NAPTIMER_GUI_H

#include <nappgui.h>

typedef struct _App App;

App *nt_gui_create(const char *db_path);
void nt_gui_destroy(App **app);
void nt_gui_update(App *app, const real64_t prtime, const real64_t ctime);

#endif
