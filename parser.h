#ifndef PARSER_SENTRY
#define PARSER_SENTRY
#include "lexer.h"


enum ast_type {
    ast_type_subshell,
    ast_type_command,
    ast_type_pipe,
    ast_type_redirect,
    ast_type_logical,   
    ast_type_sequence,
    ast_type_background
};

typedef struct ast_node ast_node;

typedef struct {
    ast_node *child;
} ast_subshell;

typedef struct {
    char **argv;
} ast_command;

typedef struct {
    ast_node *left, *right;
} ast_pipe;

typedef struct {
    enum token_type type;
    char *filename;
    ast_node *child;
} ast_redirect;

typedef struct {
    /* todo: rename */
    enum token_type operator;
    ast_node *left, *right;
} ast_logical;

typedef struct {
    int len, capacity;
    ast_node **children;
} ast_sequence;

typedef struct {
    ast_node *child;
} ast_background;

struct ast_node {
    enum ast_type type;
    union {
        ast_subshell subshell;
        ast_command command;
        ast_pipe pipe;
        ast_redirect redirect;
        ast_logical logical;
        ast_sequence sequence;
        ast_background background;
    };
};

/* todo ast_free */
enum parse_error { unexpected_end = -1, unexpected_token = -2 };
enum parse_error parse(ast_node **ast, token_item *tokens, token_item **invalid);

#endif
