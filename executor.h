#ifndef EXECUTOR_SENTRY
#define EXECUTOR_SENTRY
#include "parser.h"
#include "shell.h"


void execute(shell *sh, const ast_list_node *stmts);

#endif 
