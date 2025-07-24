#include <stdlib.h>
#include "lexer.h"


void print_tokens(token_list *lst)
{
    token_item *item = lst->head;
    while (item != NULL) {
        printf("([%s] %s)%c", item->value,
               lexer_get_token_name(item->type),
               item->next == NULL ? '\n' : ' ');
        item = item->next;
    }
}

int main(int argc, const char **argv)
{
    lexer lex;
    token_list tokens;
    int ch;

    lexer_init(&lex);
    for (;;) {
        printf("> ");
        lexer_start(&lex, &tokens);
        while ((ch = fgetc(stdin)) != EOF) {
            lexer_feed(&lex, ch);
            if (lex.eol) {
                break;
            }
        }
        lexer_end(&lex);
        if (lex.status != 0) {
            lexer_print_error(&lex, argv[0], stderr);
        }
        print_tokens(&tokens);
        token_list_free(&tokens);
        if (ch == EOF) {
            putchar('\n');
            break;
        }
    }
    lexer_free(&lex);
    return 0;
}
