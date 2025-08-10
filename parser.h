#ifndef PARSER_SENTRY
#define PARSER_SENTRY
#include "lexer.h"


enum ast_type {
    ast_type_command,
    ast_type_subshell,
    ast_type_redirection,
    ast_type_pipeline,
    ast_type_logical,   
    ast_type_background
};

typedef struct ast_node ast_node;

typedef struct {
    char **argv;
} ast_command;

typedef struct child_item_tag {
    ast_node *node;
    struct child_item_tag *next;
} ast_list_node;

typedef struct {
    ast_list_node *statements;
} ast_subshell;

enum redir_type {
    redir_in = token_redir_in,
    redir_out = token_redir_out,
    redir_append = token_redir_append
};

typedef struct redir_item_tag {
    enum redir_type type;
    int src_fd, target_fd;
    const char *filename;
    struct redir_item_tag *next;
} redir_entry;

typedef struct {
    redir_entry *entries;
    ast_node *child;
} ast_redirection;

typedef struct {
    enum token_type type;
    ast_node *left, *right;
} ast_logical;

typedef struct {
    ast_list_node *chain;
} ast_pipeline;

typedef struct {
    ast_node *child;
} ast_background;

struct ast_node {
    enum ast_type type;
    union {
        ast_command command;
        ast_subshell subshell;
        ast_redirection redirection;
        ast_logical logical;
        ast_pipeline pipeline;
        ast_background background;
    };
};

int parse(ast_list_node **plist, token_item *tokens, token_item **invalid);
void ast_list_free(ast_list_node *head);

#endif
