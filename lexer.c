#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>


static int str_to_int(const char *str, int *res)
{
    int is_negative;
    long num; 

    is_negative = *str == '-';
    if (is_negative) {
        str++;
    }
    if (*str == '\0') {
        errno = EINVAL;
        return -1;
    }
    num = 0;
    while (*str != '\0') {
        if (!isdigit(*str)) {
            errno = EINVAL;
            return -1;
        }
        num = num * 10 + (*str - '0');
        if ((num > INT_MAX) || (is_negative && num-1 > INT_MAX)) {
            errno = ERANGE;
            return -1;
        }
        str++;
    }
    if (is_negative) {
        num = -num;
    }
    *res = num;
    return 0;
}

void tokens_free(token_item *head)
{
    while (head != NULL) {
        token_item *tmp = head;

        head = head->next;
        if (tmp->type == token_word) {
            free(tmp->str_val);
        }
        free(tmp);
    }
}

static void append_token(
    token_item **phead, token_item **ptail, token_item *token)
{
    if (*ptail != NULL) {
        (*ptail)->next = token;
    } else {
        *phead = token;
    }
    *ptail = token;
}

static void append_int_token(
    token_item **phead, token_item **ptail,
    enum token_type type, int int_val)
{
    token_item *token;

    token = malloc(sizeof(token_item));
    token->type = type;
    token->int_val = int_val;
    token->next = NULL;
    append_token(phead, ptail, token);
}

static void append_str_token(
    token_item **phead, token_item **ptail,
    enum token_type type, const char *str_val)
{
    token_item *token;

    token = malloc(sizeof(token_item));
    token->type = type;
    token->str_val = strdup(str_val);
    token->next = NULL;
    append_token(phead, ptail, token);
}

static void append_empty_token(
    token_item **phead, token_item **ptail,
    enum token_type type)
{
    token_item *token;

    token = calloc(1, sizeof(token_item));
    token->type = type;
    append_token(phead, ptail, token);
}

static void append_in_str_token(lexer *l, enum token_type type, char ch)
{
    l->have_token = 1;
    l->type = token_word;
    strbuf_append(&l->str_val, ch);
}

static void set_empty_token(lexer *l, enum token_type type)
{
    l->have_token = 1;
    l->type = type;
}

static void set_int_token(lexer *l, enum token_type type, int int_val)
{
    l->have_token = 1;
    l->type = type;
    l->int_val = int_val;
}

static void save_cur_token(lexer *l)
{
    switch (l->type) {
    case token_word:
        append_str_token(&l->head, &l->tail, l->type, l->str_val.chars);
        break;
    case token_bg:              case token_and:       
    case token_pipe:            case token_or:       
    case token_semicolon:       case token_lparen:
    case token_rparen:
        append_empty_token(&l->head, &l->tail, l->type);
        break;
    case token_redir_in:        case token_redir_out:
    case token_redir_append:
        append_int_token(&l->head, &l->tail, l->type, l->int_val);
        break;
    }
    l->have_token = 0;
    strbuf_clear(&l->str_val);
}

void lexer_init(lexer *l)
{
    strbuf_init(&l->str_val, 64);
    l->line_num = 0;
}

void lexer_free(lexer *l)
{
    strbuf_free(&l->str_val);
}

void lexer_start(lexer *l)
{
    l->head = l->tail = NULL;
    strbuf_clear(&l->str_val);
    l->have_token = l->eol = 0;
    l->in_squote = l->in_dquote = l->in_escape = 0;
    l->line_num++;
    l->char_num = 0;
}

enum lexer_error lexer_end(lexer *l, token_item **phead)
{
    *phead = l->head;
    if (l->in_squote || l->in_dquote) {
        return lexer_unclosed_quote;
    }
    if (l->in_escape) {
        return lexer_unfinished_escaping;
    }
    if (l->have_token) {
        save_cur_token(l);
        *phead = l->head;
    }
    return lexer_ok;
}

static void escaping(lexer *l, char ch)
{
    if (ch == '\n') {
        return;
    }
    append_in_str_token(l, token_word, ch);
    l->in_escape = 0;
}

static void in_quote(lexer *l, int *quote_flag, char quote, char ch)
{
    l->have_token = 1;
    if (ch == quote) {
        *quote_flag = 0;
    } else {
        append_in_str_token(l, token_word, ch);
    }
}

static void greater_operator(lexer *l)
{
    if (!l->have_token) {
        set_int_token(l, token_redir_out, 1);
        return;
    }
    if (l->type == token_redir_out) {
        set_int_token(l, token_redir_append, l->int_val);
        save_cur_token(l);
        return;
    }
    if (l->type == token_word) {
        int status, int_val;

        status = str_to_int(l->str_val.chars, &int_val);
        if (status == 0 && int_val >= 0) {
            set_int_token(l, token_redir_out, int_val);
            return;
        }
    }
    save_cur_token(l);
    set_int_token(l, token_redir_out, 1);
}

static void less_operator(lexer *l)
{
    if (!l->have_token) {
        set_int_token(l, token_redir_in, 0);
        return;
    }
    if (l->type == token_word) {
        int status, int_val;

        status = str_to_int(l->str_val.chars, &int_val);
        if (status == 0 && int_val >= 0) {
            set_int_token(l, token_redir_in, int_val);
            return;
        }
    }
    save_cur_token(l);
    set_int_token(l, token_redir_in, 0);
}

static void single_operator(
    lexer *l, enum token_type type)
{
    if (l->have_token) {
        save_cur_token(l);
    }
    set_empty_token(l, type);
    save_cur_token(l);
}

static void doubleable_operator(
    lexer *l, enum token_type single_op, enum token_type double_op)
{
    if (!l->have_token) {
        set_empty_token(l, single_op);
    } else if (l->type == single_op) {
        set_empty_token(l, double_op);
        save_cur_token(l);
    } else {
        save_cur_token(l);
        set_empty_token(l, single_op);
    }
}

static void word(lexer *l, char ch)
{
    if (l->have_token && l->type != token_word) {
        save_cur_token(l);
    }
    append_in_str_token(l, token_word, ch);
}

static void space(lexer *l)
{
    if (l->have_token) {
        save_cur_token(l);
    }
}

void lexer_feed(lexer *l, char ch)
{
    l->char_num++;
    if (l->in_escape) {
        escaping(l, ch);
    } else if (ch == '\\') {
        l->in_escape = 1;
    } else if (ch == '\n') {
        l->eol = 1;
    } else if (l->in_dquote) {
        in_quote(l, &l->in_dquote, '"', ch);
    } else if (ch == '"') {
        l->in_dquote = 1;
    } else if (l->in_squote) {
        in_quote(l, &l->in_squote, '\'', ch);
    } else if (ch == '\'') {
        l->in_squote = 1;
    } else if (isspace(ch)) {
        space(l);
    } else if (ch == '&') {
        doubleable_operator(l, token_bg, token_and);
    } else if (ch == '|') {
        doubleable_operator(l, token_pipe, token_or);
    } else if (ch == ';') {
        single_operator(l, token_semicolon);
    } else if (ch == '>') {
        greater_operator(l);
    } else if (ch == '<') {
        less_operator(l);
    } else if (ch == '(') {
        single_operator(l, token_lparen);
    } else if (ch == ')') {
        single_operator(l, token_rparen);
    } else {
        word(l, ch);
    }
}

const char* token_name(enum token_type type)
{
    switch (type) {
    case token_word:
        return "word";
    case token_bg:
        return "&";
    case token_and:
        return "&&";
    case token_pipe:
        return "|";
    case token_or:
        return "||";
    case token_semicolon:
        return ";";
    case token_lparen:      
        return "(";
    case token_rparen:     
        return ")";
    case token_redir_in:     
        return "<";
    case token_redir_out:     
        return ">";
    case token_redir_append:  
        return ">>";
    }
    return NULL;
}

const char* lexer_error_msg(enum lexer_error err)
{
    switch (err) {
    case lexer_ok:
        return "OK";
    case lexer_unclosed_quote:
        return "unclosed quote";
    case lexer_unfinished_escaping:
        return "unfinished escaping";
    }
    return NULL;
}

int is_token_type(const token_item *token, int types)
{
    return token != NULL && types & token->type;
}

int is_token_redir(const token_item *token)
{
    return is_token_type(
        token,
        token_redir_in | token_redir_out | token_redir_append
    );
}
