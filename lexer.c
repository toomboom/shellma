#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"


static void token_list_init(token_list *lst)
{
    lst->head = lst->tail = NULL;
}

void token_list_free(token_list *lst)
{
    while (lst->head != NULL) {
        token_item *tmp = lst->head;
        lst->head = lst->head->next;
        free(tmp->value);
        free(tmp);
    }
}

static void token_list_push(
    token_list *lst, enum token_type type, const char *value)
{
    token_item *tmp;

    tmp = malloc(sizeof(token_item));
    tmp->type = type;
    tmp->value = strdup(value);
    tmp->next = NULL;
    if (lst->tail != NULL) {
        lst->tail->next = tmp;
    } else {
        lst->head = tmp;
    }
    lst->tail = tmp;
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

void lexer_start(lexer *l, token_list *list)
{
    token_list_init(list);
    l->list = list;
    strbuf_clear(&l->cur);
    l->type = token_word;
    l->have_token = l->eol = 0;
    l->in_squote = l->in_dquote = l->in_escape = 0;
    l->status = lexer_ok;
    l->line_num++;
    l->char_num = 0;
}

static void save_token(lexer *l)
{
    token_list_push(l->list, l->type, l->cur.chars);
    l->have_token = 0;
    strbuf_clear(&l->cur);
}

void lexer_end(lexer *l)
{
    if (l->in_squote || l->in_dquote) {
        l->status = lexer_unclosed_quote;
        return;
    }
    if (l->in_escape) {
        l->status = lexer_unfinished_escaping;
        return;
    }
    if (l->have_token) {
        save_token(l);
    }
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
        process_doubleable_operator(l, token_great, token_append, ch);
    } else if (ch == '<') {
        process_single_operator(l, token_less, ch);
    } else if (ch == '(') {
        process_single_operator(l, token_lparen, ch);
    } else if (ch == ')') {
        process_single_operator(l, token_rparen, ch);
    } else {
        process_alpha(l, ch);
    }
}

const char* lexer_get_token_name(enum token_type type)
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
    case token_great:     
        return "great";
    case token_less:     
        return "less";
    case token_append:  
        return "append";
    }
    return NULL;
}

const char* lexer_status_message(enum lexer_status status)
{
    switch (status) {
    case lexer_ok:
        return "OK";
    case lexer_unclosed_quote:
        return "unclosed quote";
    case lexer_unfinished_escaping:
        return "unfinished escaping";
    }
    return NULL;
}

void lexer_print_error(lexer *l, const char *program, FILE *f)
{
    fprintf(f, "%s: line %d, char %d: %s\n",
            program, l->line_num, l->char_num, lexer_status_message(l->status));
}
