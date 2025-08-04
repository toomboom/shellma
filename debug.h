#ifndef DEBUG_SENTRY
#define DEBUG_SENTRY
#include <stdio.h>
#include "lexer.h"
#include "parser.h"


void log_tokens(FILE *f, const token_item *token);
void log_ast(FILE *f, const ast_list_node *list);

#endif
