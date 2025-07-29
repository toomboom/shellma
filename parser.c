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

static void init_ast_command(ast_node **pnode, char **argv)
{
    init_ast(pnode, ast_type_command);
    (*pnode)->command.argv = argv;
}

static void init_ast_redirection(
    ast_node **pnode, enum token_type type, char *filename, ast_node *child)
{
    ast_redirect *redir;

    init_ast(pnode, ast_type_redirect);
    redir = &(*pnode)->redirect;
    redir->type = type;
    redir->filename = filename;
    redir->child = child;
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

static void init_ast_list(ast_node **plist)
{
    init_ast(plist, ast_type_list);
    (*plist)->list.head = (*plist)->list.tail = NULL;
}

static void ast_list_push(ast_node **plist, ast_node *child)
{
    ast_list_item *tmp;
    ast_list *list = &(*plist)->list;

    tmp = malloc(sizeof(ast_list_item));
    tmp->child = child;
    tmp->next = NULL;
    if (list->tail != NULL) {
        list->tail->next = tmp;
    } else {
        list->head = tmp;
    }
    list->tail = tmp;
}

static void ast_list_lazy_push(ast_node **plist, ast_node *child)
{
    if (*plist == NULL) {
        *plist = child;
        return;
    }
    if ((*plist)->type != ast_type_list) {
        ast_node *tmp = *plist;

        init_ast_list(plist);
        ast_list_push(plist, tmp);
    }
    ast_list_push(plist, child);
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

static void parse_command(ast_node **pnode, token_item **ptoken)
{
    token_item *cur;
    char **argv;
    int argc = 0, i;

    cur = *ptoken;
    while (cur != NULL && cur->type == token_word) {
        argc++;
        cur = cur->next;
    }
    argv = malloc(sizeof(char *) * (argc + 1));
    for (i = 0; i < argc; i++) {
        argv[i] = (*ptoken)->value;
        *ptoken = (*ptoken)->next;
    }
    argv[argc] = NULL;
    init_ast_command(pnode, argv);
}

static int parse_redirection(ast_node **pnode, token_item **ptoken)
{
    enum token_type redir_type;
    int have_redir;

    parse_command(pnode, ptoken);
    have_redir = is_token_type(
        *ptoken, 3, token_redir_in,
        token_redir_out, token_redir_append
    );
    if (!have_redir) {
        return 0; 
    }
    redir_type = (*ptoken)->type;
    *ptoken = (*ptoken)->next;
    if (!is_token_type(*ptoken, 1, token_word)) {
        return 1;
    }
    init_ast_redirection(pnode, redir_type, (*ptoken)->value, *pnode);
    *ptoken = (*ptoken)->next;
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
        return parse_redirection(pnode, ptoken);
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

#if 0
static int _parse_redirection(ast_node **pleft, token_item **ptoken)
{
    enum token_type redir_type;
    int status, done;

    status = parse_pipe(pleft, ptoken);
    done = status != 0 || !is_token_type(
        *ptoken, 3, token_redir_in,
        token_redir_out, token_redir_append
    );
    if (done) {
        return status;
    }
    redir_type = (*ptoken)->type;
    *ptoken = (*ptoken)->next;
    if (!is_token_type(*ptoken, 1, token_word)) {
        return 1;
    }
    init_ast_redirection(pleft, redir_type, (*ptoken)->value, *pleft);
    *ptoken = (*ptoken)->next;
    return 0;
}
#endif

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
    ast_node **plist, token_item **ptoken,
    int (*parse_callback)(ast_node **, token_item **))
{
    ast_node *child;
    int status;

    *plist = NULL;
    do {
        status = parse_callback(&child, ptoken);
        if (status != 0) {
            return status;
        }
        ast_list_lazy_push(plist, child);
    } while (is_token_type(*ptoken, 2, token_word, token_lparen));
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
