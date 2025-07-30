#include <stdlib.h>
#include <stdarg.h>
#include "parser.h"
#include <assert.h>


static void init_ast(ast_node **pnode, enum ast_type type)
{
    *pnode = calloc(1, sizeof(ast_node));
    (*pnode)->type = type;
}

static void init_ast_subshell(ast_node **pnode, ast_node *child)
{
    init_ast(pnode, ast_type_subshell);
    (*pnode)->subshell.child = child;
}

static void init_ast_command(ast_node **pnode, char **argv, redir_item *redirs)
{
    init_ast(pnode, ast_type_command);
    (*pnode)->command.argv = argv;
    (*pnode)->command.redirs = redirs;
}

static void redir_list_append(
    redir_item **phead, redir_item **ptail,
    enum token_type type, const char *filename, int fd)
{
    redir_item *item;

    item = malloc(sizeof(redir_item));
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

static void redir_list_free(redir_item *head) {
    while (head != NULL) {
        redir_item *tmp = head;
        head = head->next;
        free(tmp);
    }
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

static void init_ast_pipe(ast_node **pnode, ast_node *left, ast_node *right)
{
    ast_pipe *pipe;

    init_ast(pnode, ast_type_pipe);
    pipe = &(*pnode)->pipe;
    pipe->left = left;
    pipe->right = right;
}

static void init_ast_list(ast_node **plist, child_item *children)
{
    init_ast(plist, ast_type_list);
    (*plist)->list.children = children;
}

static void child_list_append(
    child_item **phead, child_item **ptail, ast_node *child)
{
    child_item *tmp;

    tmp = malloc(sizeof(child_item));
    tmp->child = child;
    tmp->next = NULL;
    if (*ptail != NULL) {
        (*ptail)->next = tmp;
    } else {
        *phead = tmp;
    }
    *ptail = tmp;
}

void ast_free(ast_node *ast);

static void child_list_free(child_item *head)
{
    while (head != NULL) {
        child_item *tmp = head;
        head = head->next;
        ast_free(tmp->child);
        free(tmp);
    }
}

int is_token_type(const token_item *token, int count, ...)
{
    va_list args;
    int is_match = 0;

    if (token == NULL) {
        return 0;
    }
    va_start(args, count);
    while (count > 0) {
        if (token->type == va_arg(args, enum token_type)) {
            is_match = 1;
            break;
        }
        count--;
    }
    va_end(args);
    return is_match;
}

static void parse_argv(char ***pargv, token_item **ptoken)
{
    token_item *cur;
    int argc = 0, i;

    cur = *ptoken;
    while (cur != NULL && cur->type == token_word) {
        argc++;
        cur = cur->next;
    }
    *pargv = malloc(sizeof(char *) * (argc + 1));
    for (i = 0; i < argc; i++) {
        (*pargv)[i] = (*ptoken)->value;
        *ptoken = (*ptoken)->next;
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
    redir_item **phead, redir_item **ptail, token_item **ptoken)
{
    const char *filename;
    enum token_type type;
    int fd;

    type = (*ptoken)->type;
    *ptoken = (*ptoken)->next;
    if (!is_token_type(*ptoken, 1, token_word)) {
        return 1;
    }
    filename = (*ptoken)->value;
    fd = get_redir_fd(type);
    redir_list_append(phead, ptail, type, filename, fd);
    *ptoken = (*ptoken)->next;
    return 0;
}

static int parse_command(ast_node **pnode, token_item **ptoken)
{
    redir_item *head = NULL, *tail = NULL;
    char **argv;
    int status;

    parse_argv(&argv, ptoken);
    while (is_token_type(*ptoken, 3,
           token_redir_in, token_redir_out, token_redir_append)) {
        status = parse_redir(&head, &tail, ptoken);
        if (status != 0) {
            free(argv);
            redir_list_free(head);
            return 1;
        }
    }
    init_ast_command(pnode, argv, head);
    return 0;
}

static int parse_background(ast_node **pchild, token_item **ptoken);
static int parse_list(
    ast_node **plist, token_item **ptoken,
    int (*parse_callback)(ast_node **, token_item **));

static int parse_factor(ast_node **pnode, token_item **ptoken)
{
    int status;

    if (*ptoken == NULL) {
        return 1;
    } else if ((*ptoken)->type == token_word) {
        return parse_command(pnode, ptoken);
    } else if ((*ptoken)->type == token_lparen) {
        *ptoken = (*ptoken)->next;
        status = parse_list(pnode, ptoken, parse_background);
        if (status != 0 || !is_token_type(*ptoken, 1, token_rparen)) {
            return 1;
        }
        *ptoken = (*ptoken)->next;
        init_ast_subshell(pnode, *pnode);
        return 0;
    } else {
        return 1; 
    }
}

static int parse_pipe(ast_node **pleft, token_item **ptoken)
{
    ast_node *right;
    int status;

    status = parse_factor(pleft, ptoken);
    if (status != 0) {
        return status;
    }
    while (is_token_type(*ptoken, 1, token_pipe)) {
        *ptoken = (*ptoken)->next;
        status = parse_factor(&right, ptoken);
        if (status != 0) {
            return status;
        }
        init_ast_pipe(pleft, *pleft, right);
    }
    return 0;
}

static int parse_logical(ast_node **pleft, token_item **ptoken)
{
    enum token_type type;
    ast_node *right;
    int status;

    status = parse_pipe(pleft, ptoken);
    if (status != 0) {
        return status;
    }
    while (is_token_type(*ptoken, 2, token_and, token_or)) {
        type = (*ptoken)->type;
        *ptoken = (*ptoken)->next;
        status = parse_pipe(&right, ptoken);
        if (status != 0) {
            return status;
        }
        init_ast_logical(pleft, type, *pleft, right);
    }
    return 0;
}

static int parse_list(
    ast_node **pnode, token_item **ptoken,
    int (*parse_callback)(ast_node **, token_item **))
{
    child_item *head = NULL, *tail = NULL;
    ast_node *child;
    int status;

    do {
        status = parse_callback(&child, ptoken);
        if (status != 0) {
            child_list_free(head);
            return status;
        }
        child_list_append(&head, &tail, child);
    } while (is_token_type(*ptoken, 2, token_word, token_lparen));
    if (head == tail) {
        *pnode = head->child;
        free(head);
    } else {
        init_ast_list(pnode, head);
    }
    return 0;
}

static int parse_semicolon(ast_node **pchild, token_item **ptoken)
{
    int status;

    status = parse_list(pchild, ptoken, &parse_logical);
    if (status != 0) {
        return status;
    }
    if (is_token_type(*ptoken, 1, token_semicolon)) {
        *ptoken = (*ptoken)->next;
    }
    return 0;
}

static int parse_background(ast_node **pchild, token_item **ptoken)
{
    int status;

    status = parse_list(pchild, ptoken, &parse_semicolon);
    if (status != 0) {
        return status;
    }
    if (is_token_type(*ptoken, 1, token_bg)) {
        *ptoken = (*ptoken)->next;
        init_ast_background(pchild, *pchild);
    }
    return 0;
}

enum parse_error parse(ast_node **ast, token_item *tokens, token_item **invalid)
{
    int status;

    *ast = NULL;
    if (tokens == NULL) {
        return 0;
    }
    status = parse_list(ast, &tokens, &parse_background);
    if (status != 0 || tokens != NULL) {
        *invalid = tokens; 
        return tokens == NULL ? unexpected_end : unexpected_token;
    } 
    return 0;
}

void ast_free(ast_node *ast)
{
    if (ast == NULL) {
        return;
    }
    switch (ast->type) {
        case ast_type_command:
            redir_list_free(ast->command.redirs);
            free(ast->command.argv);
            break;
        case ast_type_subshell:
            ast_free(ast->subshell.child);
            break;
        case ast_type_pipe:
            ast_free(ast->pipe.left);
            ast_free(ast->pipe.right);
            break;
        case ast_type_logical:   
            ast_free(ast->logical.left);
            ast_free(ast->logical.right);
            break;
        case ast_type_list:
            child_list_free(ast->list.children);
            break;
        case ast_type_background:
            ast_free(ast->background.child);
            break;
    }
    free(ast);
}
