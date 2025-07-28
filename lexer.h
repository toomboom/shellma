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
    token_redir_in,     /* >  */
    token_redir_out,    /* <  */
    token_redir_append, /* >> */
    /* todo: maybe just token_[in|out|append] */
};

typedef struct token_item token_item;
struct token_item {
    char *value;
    enum token_type type;
    token_item *next;
};
enum lexer_error {
    lexer_ok = 0,
    lexer_unclosed_quote = -1,
    lexer_unfinished_escaping = -2
};

typedef struct {
    token_item *head, *tail;
    strbuf cur;
    enum token_type type;
    int have_token, eol;
    int in_squote, in_dquote, in_escape;
    int line_num, char_num;
} lexer;

void tokens_free(token_item *tokens);
void lexer_init(lexer *l);
void lexer_free(lexer *l);
void lexer_start(lexer *l);
enum lexer_error lexer_end(lexer *l, token_item **phead);
void lexer_feed(lexer *l, char ch);
const char* get_token_name(enum token_type type);
const char* lexer_error_msg(enum lexer_error status);
#if 0
void lexer_print_error(lexer *l, const char *program, FILE *f);
#endif

#endif
