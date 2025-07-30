#ifndef PARSER_SENTRY
#define PARSER_SENTRY
#include "lexer.h"


enum ast_type {
    ast_type_command,
    ast_type_subshell,
    ast_type_pipe,
    ast_type_logical,   
    ast_type_list,
    ast_type_background
};

typedef struct ast_node ast_node;

typedef struct redir_item_tag {
    enum token_type type;
    const char *filename;
    int fd;
    struct redir_item_tag *next;
} redir_item;

typedef struct {
    redir_item *redirs;
    char **argv;
} ast_command;

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

typedef struct child_item_tag {
    ast_node *child; 
    struct child_item_tag *next;
} child_item;

typedef struct {
    child_item *children;
} ast_list;

typedef struct {
    ast_node *child;
} ast_background;

struct ast_node {
    enum ast_type type;
    union {
        ast_command command;
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
