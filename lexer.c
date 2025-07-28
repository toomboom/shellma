#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"


void tokens_free(token_item *head)
{
    while (head != NULL) {
        token_item *tmp = head;
        head = head->next;
        free(tmp->value);
        free(tmp);
    }
}

static void push_token(
    token_item **phead, token_item **ptail,
    enum token_type type, const char *value)
{
    token_item *tmp;

    tmp = malloc(sizeof(token_item));
    tmp->type = type;
    tmp->value = strdup(value);
    tmp->next = NULL;
    if (*ptail != NULL) {
        (*ptail)->next = tmp;
    } else {
        *phead = tmp;
    }
    *ptail = tmp;
}

void lexer_init(lexer *l)
{
    strbuf_init(&l->cur, 64);
    l->line_num = 0;
}

void lexer_free(lexer *l)
{
    strbuf_free(&l->cur);
}

void lexer_start(lexer *l)
{
    l->head = l->tail = NULL;
    strbuf_clear(&l->cur);
    l->type = token_word;
    l->have_token = l->eol = 0;
    l->in_squote = l->in_dquote = l->in_escape = 0;
    l->line_num++;
    l->char_num = 0;
}

static void save_token(lexer *l)
{
    push_token(&l->head, &l->tail, l->type, l->cur.chars);
    l->have_token = 0;
    strbuf_clear(&l->cur);
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
        save_token(l);
        *phead = l->head;
    }
    return lexer_ok;
}

static void append_char(lexer *l, enum token_type type, char ch)
{
    l->have_token = 1;
    l->type = type;
    strbuf_append(&l->cur, ch);
}

static void process_escaping(lexer *l, char ch)
{
    if (ch == '\n') {
        return;
    }
    append_char(l, token_word, ch);
    l->in_escape = 0;
}

static void process_in_quote(lexer *l, int *quote_flag, char quote, char ch)
{
    l->have_token = 1;
    if (ch == quote) {
        *quote_flag = 0;
    } else {
        append_char(l, token_word, ch);
    }
}

static void process_doubleable_operator(
    lexer *l, enum token_type single_op, enum token_type double_op, char ch)
{
    if (!l->have_token) {
        append_char(l, single_op, ch);
        return;
    }
    if (l->type == single_op) {
        append_char(l, double_op, ch);
        save_token(l);
        return;
    }
    save_token(l);
    append_char(l, single_op, ch);
}

static void process_single_operator(
    lexer *l, enum token_type type, char ch)
{
    if (l->have_token) {
        save_token(l);
    }
    append_char(l, type, ch);
    save_token(l);
}

static void process_empty(lexer *l)
{
    if (l->have_token) {
        save_token(l);
    }
}

static void process_alpha(lexer *l, char ch)
{
    if (l->have_token && l->type != token_word) {
        save_token(l);
    }
    append_char(l, token_word, ch);
}

void lexer_feed(lexer *l, char ch)
{
    l->char_num++;
    if (l->in_escape) {
        process_escaping(l, ch);
    } else if (ch == '\\') {
        l->in_escape = 1;
    } else if (ch == '\n') {
        l->eol = 1; /* todo: maybe process like empty */
    } else if (l->in_dquote) {
        process_in_quote(l, &l->in_dquote, '"', ch);
    } else if (ch == '"') {
        l->in_dquote = 1;
    } else if (l->in_squote) {
        process_in_quote(l, &l->in_squote, '\'', ch);
    } else if (ch == '\'') {
        l->in_squote = 1;
    } else if (isspace(ch)) {
        process_empty(l);
    } else if (ch == '&') {
        process_doubleable_operator(l, token_bg, token_and, ch);
    } else if (ch == '|') {
        process_doubleable_operator(l, token_pipe, token_or, ch);
    } else if (ch == ';') {
        process_single_operator(l, token_semicolon, ch);
    } else if (ch == '>') {
        process_doubleable_operator(l, token_redir_out, token_redir_append, ch);
    } else if (ch == '<') {
        process_single_operator(l, token_redir_in, ch);
    } else if (ch == '(') {
        process_single_operator(l, token_lparen, ch);
    } else if (ch == ')') {
        process_single_operator(l, token_rparen, ch);
    } else {
        process_alpha(l, ch);
    }
}

const char* get_token_name(enum token_type type)
{
    switch (type) {
    case token_word:
        return "word";
    case token_bg:
        return "background";
    case token_and:
        return "and";
    case token_pipe:
        return "pipe";
    case token_or:
        return "or";
    case token_semicolon:
        return "semicolon";
    case token_lparen:      
        return "left parenthesis";
    case token_rparen:     
        return "right parenthesis";
    case token_redir_in:     
        return "redir in";
    case token_redir_out:     
        return "redir out";
    case token_redir_append:  
        return "redir append";
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

#if 0
void lexer_print_error(lexer *l, const char *program, FILE *f)
{
    fprintf(f, "%s: line %d, char %d: %s\n",
            program, l->line_num, l->char_num, lexer_status_message(l->status));
}
#endif
