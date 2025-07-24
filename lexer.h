#ifndef LEXER_SENTRY
#define LEXER_SENTRY
#include "strbuf.h"
#include <stdio.h>


enum token_type {
    token_word,
    token_bg,           /* &  */
    token_and,          /* && */
    token_pipe,         /* |  */
    token_or,           /* || */
    token_semicolon,    /* ;  */
    token_lparen,       /* (  */
    token_rparen,       /* )  */
    token_great,        /* >  */
    token_less,         /* <  */
    token_append,       /* >> */
};

typedef struct token_item token_item;
struct token_item {
    char *value;
    enum token_type type;
    token_item *next;
};

typedef struct {
    token_item *head, *tail;
} token_list;

enum lexer_status {
    lexer_ok = 0,
    lexer_unclosed_quote = -1,
    lexer_unfinished_escaping = -2
};

typedef struct {
    token_list *list;
    strbuf cur;
    enum token_type type;
    int have_token, eol;
    int in_squote, in_dquote, in_escape;
    enum lexer_status status;
    int line_num, char_num;
} lexer;

void token_list_free(token_list *lst);
void lexer_init(lexer *l);
void lexer_free(lexer *l);
void lexer_start(lexer *l, token_list *list);
void lexer_end(lexer *l);
void lexer_feed(lexer *l, char ch);
const char* lexer_get_token_name(enum token_type type);
const char* lexer_status_message(enum lexer_status status);
void lexer_print_error(lexer *l, const char *program, FILE *f);

#endif
