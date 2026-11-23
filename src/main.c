#include "cli.h"
#include "gui.h"
#include "util.h"

#include <nappgui.h>
#include <osapp/osmain.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char i_db_path[NT_PATH_LEN];

static const char *i_arg_value(int argc, char *argv[], const char *short_name, const char *long_name)
{
    int i;
    for (i = 1; i < argc; i += 1)
    {
        if ((short_name != NULL && strcmp(argv[i], short_name) == 0) ||
            (long_name != NULL && strcmp(argv[i], long_name) == 0))
        {
            if (i + 1 < argc)
                return argv[i + 1];
            return NULL;
        }
    }
    return NULL;
}

static void i_resolve_db_path(int argc, char *argv[])
{
    const char *database = i_arg_value(argc, argv, "-D", "--database");
    const char *datadir = i_arg_value(argc, argv, "-d", "--datadir");

    if (database != NULL && database[0] != '\0')
    {
        snprintf(i_db_path, sizeof(i_db_path), "%s", database);
        return;
    }
    if (datadir != NULL && datadir[0] != '\0')
    {
        snprintf(i_db_path, sizeof(i_db_path), "%s/naptimer.db", datadir);
        return;
    }
    nt_data_path(i_db_path, sizeof(i_db_path), "naptimer.db");
}

static App *i_create(void)
{
    uint32_t argc = osapp_argc();
    char **argv = (char **)osapp_argv();
    int cli;

    i_resolve_db_path((int)argc, argv);
    cli = nt_cli_is_cli((int)argc, argv);
    if (cli)
    {
        int rc = nt_cli_dispatch((int)argc, argv, i_db_path);
        exit(rc);
        return NULL;
    }

    return nt_gui_create(i_db_path);
}

static void i_destroy(App **app)
{
    nt_gui_destroy(app);
}

static void i_update(App *app, const real64_t prtime, const real64_t ctime)
{
    nt_gui_update(app, prtime, ctime);
}

osmain_sync(0.25, i_create, i_destroy, i_update, "", App)
