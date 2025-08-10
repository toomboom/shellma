#ifndef LEXER_SENTRY
#define LEXER_SENTRY
#include "strbuf.h"
#include <stdio.h>


enum token_type {
    token_word          = 1<<0,
    token_bg            = 1<<1, /* &  */
    token_and           = 1<<2, /* && */
    token_pipe          = 1<<3, /* |  */
    token_or            = 1<<4, /* || */
    token_semicolon     = 1<<5, /* ;  */
    token_lparen        = 1<<6, /* (  */
    token_rparen        = 1<<7, /* )  */
    token_redir_in      = 1<<8, /* <  */
    token_redir_out     = 1<<9, /* >  */
    token_redir_append  = 1<<10 /* >> */
};

typedef struct token_item token_item;
struct token_item {
    union {
        char *str_val;
        int int_val;
    };
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
    strbuf str_val;
    int int_val;
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
const char* token_name(enum token_type type);
const char* lexer_error_msg(enum lexer_error status);
int is_token_type(const token_item *token, int types);
int is_token_redir(const token_item *token);


#endif
