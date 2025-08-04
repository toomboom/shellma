#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#ifdef DEBUG
#include "debug.h"
#endif


int read_tokens(lexer *lex, token_item **ptoks, int *ch)
{
    printf("> ");
    lexer_start(lex);
    while ((*ch = fgetc(stdin)) != EOF) {
        lexer_feed(lex, *ch);
        if (lex->eol) {
            break;
        }
    }
    return lexer_end(lex, ptoks);
}

int main(int argc, const char **argv)
{
    lexer lex;
    ast_list_node *list;
    token_item *err_pos, *tokens;
    int status, last_char = 0;

    lexer_init(&lex);
    for (;;) {
        status = read_tokens(&lex, &tokens, &last_char);
        if (status != 0) {
            fprintf(stderr, "lexer error: %s\n", lexer_error_msg(status));
            goto cleanup;
        }

        status = parse(&list, tokens, &err_pos);
        if (status != 0) {
            fprintf(stderr, "syntax error near %s\n",
                    err_pos == NULL ? "end of line" : err_pos->value);
            goto cleanup;
        }
#ifdef DEBUG
        log_tokens(stdout, tokens);
        log_ast(stdout, list);
#endif
cleanup:
        tokens_free(tokens);
        ast_list_free(list);
        if (last_char == EOF) {
            break;
        }
    }
    putchar('\n');
    lexer_free(&lex);
    return 0;
}
