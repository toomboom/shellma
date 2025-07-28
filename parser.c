#include <stdlib.h>
#include <stdarg.h>
#include "parser.h"
#include <assert.h>

enum { sequence_init_capacity = 6 };

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
    ast_node **pnode, enum token_type operator,
    ast_node *left, ast_node *right)
{
    ast_logical *logic;

    init_ast(pnode, ast_type_logical);
    logic = &(*pnode)->logical;
    logic->operator = operator;
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

/* todo: replace array with linked list */
static void init_ast_sequence(ast_node **pnode, ast_node *child)
{
    ast_sequence *seq;

    init_ast(pnode, ast_type_sequence);
    seq = &(*pnode)->sequence;
    seq->capacity = sequence_init_capacity;
    seq->children = malloc(sizeof(ast_node *) * sequence_init_capacity);
    seq->children[0] = child;
    seq->len = 1;
}

static void append_child(ast_node **pnode, ast_node *child)
{
    ast_sequence *seq;
    if (*pnode == NULL) {
        *pnode = child;
        return;
    }
    if ((*pnode)->type != ast_type_sequence) {
        init_ast_sequence(pnode, *pnode);
    }

    seq = &(*pnode)->sequence;
    if (seq->len >= seq->capacity) {
        seq->capacity *= 2;
        seq->children = realloc(
            seq->children,
            sizeof(ast_node *) * seq->capacity
        );
    }
    seq->children[seq->len] = child;
    seq->len++;
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

static int parse_background(ast_node **pchild, token_item **ptoken);
static int parse_sequence(
    ast_node **pseq, token_item **ptoken,
    int (*parse_callback)(ast_node **, token_item **));

static int parse_factor(ast_node **pnode, token_item **ptoken)
{
    int status;

    if (*ptoken == NULL) {
        return 1;
    } else if ((*ptoken)->type == token_word) {
        parse_command(pnode, ptoken);
        return 0;
    } else if ((*ptoken)->type == token_lparen) {
        *ptoken = (*ptoken)->next;
        status = parse_sequence(pnode, ptoken, parse_background);
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

static int parse_redirection(ast_node **pleft, token_item **ptoken)
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

static int parse_logical(ast_node **pleft, token_item **ptoken)
{
    enum token_type operator;
    ast_node *right;
    int status;

    status = parse_redirection(pleft, ptoken);
    if (status != 0) {
        return status;
    }
    while (is_token_type(*ptoken, 2, token_and, token_or)) {
        operator = (*ptoken)->type;
        *ptoken = (*ptoken)->next;
        status = parse_redirection(&right, ptoken);
        if (status != 0) {
            return status;
        }
        init_ast_logical(pleft, operator, *pleft, right);
    }
    return 0;
}

static int parse_sequence(
    ast_node **pseq, token_item **ptoken,
    int (*parse_callback)(ast_node **, token_item **))
{
    ast_node *child;
    int status;

    *pseq = NULL;
    do {
        status = parse_callback(&child, ptoken);
        if (status != 0) {
            return status;
        }
        append_child(pseq, child);
    } while (is_token_type(*ptoken, 1, token_word));
    return 0;
}

static int parse_semicolon(ast_node **pchild, token_item **ptoken)
{
    int status;

    status = parse_sequence(pchild, ptoken, &parse_logical);
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

    status = parse_sequence(pchild, ptoken, &parse_semicolon);
    if (status != 0) {
        return status;
    }
    if (is_token_type(*ptoken, 1, token_bg)) {
        *ptoken = (*ptoken)->next;
        init_ast_background(pchild, *pchild);
    }
    return 0;
}

/* todo: const token_item * */
enum parse_error parse(ast_node **ast, token_item *tokens, token_item **invalid)
{
    int status;

    *ast = NULL;
    if (tokens == NULL) {
        return 0;
    }
    status = parse_sequence(ast, &tokens, &parse_background);
    if (status != 0 || tokens != NULL) {
        *invalid = tokens; 
        return tokens == NULL ? unexpected_end : unexpected_token;
    } 
    return 0;
}

#if 0
static int parse_helper(ast_node **pseq, token_item **ptoken)
{
    int status, count = 0;

    /* while (is_token_type(*ptoken, 1, token_word)) { */
    while (!is_sequence_end(*ptoken)) {
        status = parse_sequence_step(pseq, ptoken, &parse_background, &count);
        if (status != 0) {
            return status;
        }
    }
    return 0;
}

static int _parse_sequence(ast_node **pseq, token_item **ptoken)
{
    int status;

    status = parse_sequence_step(pseq, ptoken, &parse_pipe, &count);
    if (status != 0) {
        return status;
    }
    while (is_token_type(*ptoken, 1, token_semicolon)) {
        *ptoken = (*ptoken)->next;
        if (is_sequence_end(*ptoken)) {
            break;
        }
        status = parse_sequence_step(pseq, ptoken, &parse_pipe, &count);
        if (status != 0) {
            return status;
        }
    }
    return 0;
}
static int parse_background(ast_node **pchild, token_item **ptoken)
{
    int status;

    status = parse_sequence(pchild, ptoken);
    if (status != 0) {
        return status;
    }
    if (is_token_type(*ptoken, 1, token_bg)) {
        *ptoken = (*ptoken)->next;
        init_ast_background(pchild, *pchild);
    }
    return 0;
}



static int parse_sequence(ast_node **pseq, token_item **ptoken)
{
    ast_node *child;
    int status;

    *pseq = NULL;
    do {
        status = parse_pipe(&child, ptoken);
        if (status != 0) {
            return status;
        }
        append_child(pseq, child);
        if (is_token_type(*ptoken, 1, token_semicolon)) {
            *ptoken = (*ptoken)->next;
        }
    } while (is_token_type(*ptoken, 1, token_word));
    return 0;
}

static int parse_backgrounds(ast_node **pseq, token_item **ptoken)
{
    ast_node *child;
    int status;

    *pseq = NULL;
    do {
        status = parse_sequence(&child, ptoken);
        if (status != 0) {
            return status;
        }
        if (is_token_type(*ptoken, 1, token_bg)) {
            *ptoken = (*ptoken)->next;
            init_ast_background(&child, child);
        }
        append_child(pseq, child);
    } while (is_token_type(*ptoken, 1, token_word));


    *pseq = NULL;
    status = parse_sequence(&child, ptoken);
    if (status != 0) {
        return status;
    }
    while (is_token_type(*ptoken, 1, token_bg)) {
        *ptoken = (*ptoken)->next;
        init_ast_background(&child, child);
        append_child(pseq, child);
        if (*ptoken == NULL) {
            break;
        }
    }
    append_child(pseq, child);

    return 0;
}

#if 0
static int parse_sequence_step(
    ast_node **pseq, token_item **ptoken,
    int (*parse_callback)(ast_node **pseq, token_item **ptoken),
    int *count
){
    int status;
    ast_node *child;

    if (*count == 0) {
        (*count)++;
        return parse_callback(pseq, ptoken);
    }
    if (*count == 1) {
        init_ast_sequence(pseq, *pseq);
    }
    if (*count >= 1) {
        status = parse_callback(&child, ptoken);
        if (status != 0) {
            return status;
        }
        add_child_to_sequence(pseq, child);
    }
    (*count)++;
    return 0;
}
/* todo: rename */
static void add_child_to_sequence(ast_node **pnode, ast_node *child)
{
    ast_sequence *seq = &(*pnode)->sequence;

    if (seq->len >= seq->capacity) {
        seq->capacity *= 2;
        seq->children = realloc(
            seq->children,
            sizeof(ast_node *) * seq->capacity
        );
    }
    seq->children[seq->len] = child;
    seq->len++;
}

#if 0
static int parse_redirection(ast_node **pleft, token_item **ptoken)
{
    enum token_type redir_type;
    int status, done;

    status = parse_factor(pleft, ptoken);
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

#endif



#endif
