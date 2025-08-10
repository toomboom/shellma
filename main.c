#include <stdlib.h>
#include <errno.h>
#include "wrappers.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"
#ifdef DEBUG
#include "debug.h"
#endif


int read_tokens(lexer *lex, token_item **ptoks, int *ch)
{
    printf("> ");
    lexer_start(lex);
    for (;;) {
        errno = 0;
        *ch = fgetc(stdin);
        if (*ch != EOF) {
            lexer_feed(lex, *ch);
            if (lex->eol) {
                break;
            }
        } else if (errno == 0) {
            break;
        }
    }
    return lexer_end(lex, ptoks);
}

int main(int argc, const char **argv)
{
    lexer lex;
    ast_list_node *statements;
    token_item *err_pos, *tokens;
    int status, last_char = 0;

    lexer_init(&lex);
    for (;;) {
        status = read_tokens(&lex, &tokens, &last_char);
        if (status != 0) {
            fprintf(stderr, "lexer error: %s\n", lexer_error_msg(status));
            goto cleanup;
        }

        status = parse(&statements, tokens, &err_pos);
        if (status != 0) {
            fprintf(stderr, "syntax error near %s\n",
                    err_pos == NULL ? "end of line" : token_name(err_pos->type));
            goto cleanup;
        }
        status = execute(statements); 
        printf("Status=%d\n", status);
#ifdef DEBUG
        putchar('\n');
        log_tokens(stdout, tokens);
        log_ast(stdout, statements);
#endif
cleanup:
        tokens_free(tokens);
        ast_list_free(statements);
        if (last_char == EOF) {
            break;
        }
    }
    putchar('\n');
    lexer_free(&lex);
    return 0;
}
