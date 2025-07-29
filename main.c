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
    ast_node *ast;
    token_item *invalid, *tokens;
    int status, last_char = 0;

    lexer_init(&lex);
    for (;;) {
        status = read_tokens(&lex, &tokens, &last_char);
        if (status != 0) {
            fprintf(stderr, "lexer error: %s\n", lexer_error_msg(status));
            continue;
        }

        status = parse(&ast, tokens, &invalid);
        if (status != 0) {
            fprintf(stderr, "syntax error near %s\n",
                    status == unexpected_end ? "end of line" : invalid->value);
            continue;
        }
#ifdef DEBUG
        log_tokens(stdout, tokens);
        log_ast(stdout, ast);
#endif
        tokens_free(tokens);
        ast_free(ast);
        if (last_char == EOF) {
            break;
        }
    }
    putchar('\n');
    lexer_free(&lex);
    return 0;
}
