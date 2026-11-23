#ifndef NAPTIMER_CLI_H
#define NAPTIMER_CLI_H

#include "model.h"

int nt_cli_dispatch(int argc, char *argv[], const char *db_path);
int nt_cli_is_cli(int argc, char *argv[]);

#endif
