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
    char **argv = (char **)calloc(argc + 1, sizeof(char *));
    uint32_t i;
    int cli;
    int rc = 0;

    for (i = 0; i < argc; i += 1)
    {
        char_t buf[1024];
        osapp_argv(i, buf, sizeof(buf));
        argv[i] = (char *)nt_strdup(buf);
    }
    argv[argc] = NULL;

    i_resolve_db_path((int)argc, argv);
    cli = nt_cli_is_cli((int)argc, argv);
    if (cli)
    {
        rc = nt_cli_dispatch((int)argc, argv, i_db_path);
        for (i = 0; i < argc; i += 1)
            free(argv[i]);
        free(argv);
        exit(rc);
        return NULL;
    }

    for (i = 0; i < argc; i += 1)
        free(argv[i]);
    free(argv);

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
