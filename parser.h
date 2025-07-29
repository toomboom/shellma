#ifndef PARSER_SENTRY
#define PARSER_SENTRY
#include "lexer.h"


enum ast_type {
    ast_type_command,
    ast_type_redirect,
    ast_type_subshell,
    ast_type_pipe,
    ast_type_logical,   
    ast_type_list,
    ast_type_background
};

typedef struct ast_node ast_node;

typedef struct {
    char **argv;
} ast_command;

typedef struct {
    enum token_type type;
    char *filename;
    ast_node *child;
} ast_redirect;

typedef struct {
    ast_node *child;
} ast_subshell;

typedef struct {
    ast_node *left, *right;
} ast_pipe;

typedef struct {
    enum token_type type;
    ast_node *left, *right;
} ast_logical;

typedef struct ast_list_item ast_list_item;
struct ast_list_item {
    ast_node *child; 
    ast_list_item *next;
};

typedef struct {
    ast_list_item *head, *tail;
} ast_list;

typedef struct {
    ast_node *child;
} ast_background;

struct ast_node {
    enum ast_type type;
    union {
        ast_command command;
        ast_redirect redirect;
        ast_subshell subshell;
        ast_pipe pipe;
        ast_logical logical;
        ast_list list;
        ast_background background;
    };
};

enum parse_error { unexpected_end = -1, unexpected_token = -2 };
enum parse_error parse(ast_node **ast, token_item *tokens, token_item **invalid);
void ast_free(ast_node *ast);

#endif
