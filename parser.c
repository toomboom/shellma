#include <stdlib.h>
#include <stdarg.h>
#include "parser.h"


static void ast_node_free(ast_node *node);
static int parse_statements(ast_list_node **phead, token_item **pcur);

static void redir_list_append(
    redir_entry **phead, redir_entry **ptail,
    enum token_type type, const char *filename, int fd)
{
    redir_entry *item;

    item = malloc(sizeof(redir_entry));
    item->type = type;
    item->filename = filename;
    item->fd = fd;
    item->next = NULL;
    if (*ptail != NULL) {
        (*ptail)->next = item;
    } else {
        *phead = item;
    }
    *ptail = item;
}

static void redir_list_free(redir_entry *head) {
    while (head != NULL) {
        redir_entry *tmp = head;
        head = head->next;
        free(tmp);
    }
}

static void ast_list_append(
    ast_list_node **phead, ast_list_node **ptail, ast_node *node)
{
    ast_list_node *tmp;

    tmp = malloc(sizeof(ast_list_node));
    tmp->node = node;
    tmp->next = NULL;
    if (*ptail != NULL) {
        (*ptail)->next = tmp;
    } else {
        *phead = tmp;
    }
    *ptail = tmp;
}

void ast_list_free(ast_list_node *head)
{
    while (head != NULL) {
        ast_list_node *tmp = head;
        head = head->next;
        ast_node_free(tmp->node);
        free(tmp);
    }
}

static void init_ast(ast_node **pnode, enum ast_type type)
{
    *pnode = calloc(1, sizeof(ast_node));
    (*pnode)->type = type;
}

static void init_ast_command(ast_node **pnode, char **argv, redir_entry *redirs)
{
    init_ast(pnode, ast_type_command);
    (*pnode)->command.argv = argv;
    (*pnode)->command.redirs = redirs;
}

static void init_ast_subshell(ast_node **pnode, ast_list_node *stmts)
{
    init_ast(pnode, ast_type_subshell);
    (*pnode)->subshell.statements = stmts;
}

static void init_ast_background(ast_node **pnode, ast_node *child)
{
    init_ast(pnode, ast_type_background);
    (*pnode)->background.child = child;
}

static void init_ast_logical(
    ast_node **pnode, enum token_type type,
    ast_node *left, ast_node *right)
{
    ast_logical *logic;

    init_ast(pnode, ast_type_logical);
    logic = &(*pnode)->logical;
    logic->type = type;
    logic->left = left;
    logic->right = right;
}

static void init_ast_pipeline(ast_node **pnode, ast_list_node *chain)
{
    init_ast(pnode, ast_type_pipeline);
    (*pnode)->pipeline.chain = chain;
}

static void ast_node_free(ast_node *node)
{
    switch (node->type) {
    case ast_type_command:
        redir_list_free(node->command.redirs);
        free(node->command.argv);
        break;
    case ast_type_subshell:
        ast_list_free(node->subshell.statements);
        break;
    case ast_type_logical:   
        ast_node_free(node->logical.left);
        ast_node_free(node->logical.right);
        break;
    case ast_type_pipeline:
        ast_list_free(node->pipeline.chain);
        break;
    case ast_type_background:
        ast_node_free(node->background.child);
        break;
    }
    free(node);
}

static void parse_argv(char ***pargv, token_item **pcur)
{
    token_item *tmp;
    int argc = 0, i;

    tmp = *pcur;
    while (token_have_type(tmp, token_word)) {
        argc++;
        tmp = tmp->next;
    }
    *pargv = malloc(sizeof(char *) * (argc + 1));
    for (i = 0; i < argc; i++) {
        (*pargv)[i] = (*pcur)->value;
        *pcur = (*pcur)->next;
    }
    (*pargv)[argc] = NULL;
}

/* todo: processing 3>, <4 and etc maybe will be added in future versions */
static int get_redir_fd(enum token_type type)
{
    switch (type) {
        case token_redir_in:
            return 0;
        case token_redir_out:
        case token_redir_append:
            return 1;
        default:
            return -1;
    }
}

static int parse_redir(
    redir_entry **phead, redir_entry **ptail, token_item **pcur)
{
    const char *filename;
    enum token_type type;
    int fd;

    type = (*pcur)->type;
    *pcur = (*pcur)->next;
    if (!token_have_type(*pcur, token_word)) {
        return -1;
    }
    filename = (*pcur)->value;
    fd = get_redir_fd(type);
    redir_list_append(phead, ptail, type, filename, fd);
    *pcur = (*pcur)->next;
    return 0;
}

static int parse_command(ast_node **pnode, token_item **pcur)
{
    redir_entry *head = NULL, *tail = NULL;
    char **argv;
    int status;

    parse_argv(&argv, pcur);
    while (token_have_type(*pcur,
           token_redir_in|token_redir_out|token_redir_append)) {
        status = parse_redir(&head, &tail, pcur);
        if (status != 0) {
            free(argv);
            redir_list_free(head);
            return status;
        }
    }
    init_ast_command(pnode, argv, head);
    return 0;
}

static int parse_factor(ast_node **pnode, token_item **pcur)
{
    int status;

    if (token_have_type(*pcur, token_word)) {
        return parse_command(pnode, pcur);
    } else if (token_have_type(*pcur, token_lparen)) {
        ast_list_node *stmts;

        *pcur = (*pcur)->next;
        status = parse_statements(&stmts, pcur);
        if (status != 0) {
            return status;
        }
        if (!token_have_type(*pcur, token_rparen)) {
            ast_list_free(stmts);
            return -1;
        }
        *pcur = (*pcur)->next;
        init_ast_subshell(pnode, stmts);
        return 0;
    } else {
        return -1; 
    }
}

static int parse_pipeline(ast_node **pnode, token_item **pcur)
{
    ast_list_node *head = NULL, *tail = NULL;
    int status;

    status = parse_factor(pnode, pcur);
    if (status != 0 || !token_have_type(*pcur, token_pipe)) {
        return status;
    }
    ast_list_append(&head, &tail, *pnode);
    while (token_have_type(*pcur, token_pipe)) {
        *pcur = (*pcur)->next;
        status = parse_factor(pnode, pcur);
        if (status != 0) {
            ast_list_free(head);
            return status;
        }
        ast_list_append(&head, &tail, *pnode);
    }
    init_ast_pipeline(pnode, head);
    return 0;
}

static int parse_logical(ast_node **pleft, token_item **pcur)
{
    ast_node *right;
    int status;

    status = parse_pipeline(pleft, pcur);
    if (status != 0) {
        return status;
    }
    while (token_have_type(*pcur, token_and | token_or)) {
        enum token_type type = (*pcur)->type;

        *pcur = (*pcur)->next;
        status = parse_pipeline(&right, pcur);
        if (status != 0) {
            return status;
        }
        init_ast_logical(pleft, type, *pleft, right);
    }
    return 0;
}

static int parse_statements(ast_list_node **phead, token_item **pcur)
{
    ast_list_node *tail = NULL;
    ast_node *node;
    int status;

    *phead = NULL;
    do {
        status = parse_logical(&node, pcur);
        if (status != 0) {
            ast_list_free(*phead);
            return status;
        }

        if (token_have_type(*pcur, token_bg)) {
            init_ast_background(&node, node);
        }
        if (token_have_type(*pcur, token_bg | token_semicolon)) {
            *pcur = (*pcur)->next;
        }
        ast_list_append(phead, &tail, node);
    } while (token_have_type(*pcur, token_word));
    return 0;
}

int parse(ast_list_node **result, token_item *tokens, token_item **err_pos)
{
    int status;

    *result = NULL;
    if (tokens == NULL) {
        return 0;
    }
    status = parse_statements(result, &tokens);
    if (status == 0 && tokens == NULL) {
        return 0;
    }
    *err_pos = tokens; 
    if (status == 0) {
        ast_list_free(*result);
    }
    *result = NULL;
    return -1;
}
