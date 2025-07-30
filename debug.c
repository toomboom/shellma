#include "debug.h"
#include "strbuf.h"


void log_tokens(FILE *file, const token_item *token)
{
    fprintf(file, "LOG: TOKENS:\n");
    while (token != NULL) {
        fprintf(file, "([%s] %s)%c", token->value, get_token_name(token->type),
                token->next == NULL ? '\n' : ' ');
        token = token->next;
    }
}

static void put_tabs(FILE *f, int count)
{
    while (count > 0) {
        printf("\t");
        count--;
    }
}

static void log_command(FILE *f, char * const *argv, const redir_item *redirs)
{
    int i;

    fprintf(f, "command: [");
    for (i = 0; argv[i] != NULL; i++) {
        fprintf(f, "%s%s", argv[i], argv[i+1] == NULL ? "" : ", ");
    }
    fprintf(f, "] ");
    while (redirs != NULL) {
        if (redirs->type == token_redir_in) {
            fprintf(f, "%s %s %d", redirs->filename,
                    get_token_name(redirs->type), redirs->fd);
        } else {
            fprintf(f, "%d %s %s", redirs->fd, get_token_name(redirs->type),
                    redirs->filename);
        }
        fprintf(f, "%s", redirs->next == NULL ? "" : ", ");
        redirs = redirs->next;
    }
    fputc('\n', f);
}

static void log_ast_helper(FILE *f, const ast_node *node, int depth);
static void log_list(FILE *f, const child_item *head, int depth)
{
    fprintf(f, "list:\n");
    while (head != NULL) {
        log_ast_helper(f, head->child, depth);
        head = head->next;
    }
}

static void log_ast_helper(FILE *f, const ast_node *node, int depth) {
    put_tabs(f, depth);
    if (node == NULL) {
        fprintf(f, "<empty>\n");
        return;
    }
    depth++;
    switch (node->type) {
        case ast_type_subshell:
            fprintf(f, "subshell:\n");
            log_ast_helper(f, node->subshell.child, depth);
            break;
        case ast_type_command:
            log_command(f, node->command.argv, node->command.redirs);
            break;
        case ast_type_logical:
            fprintf(f, "%s:\n", get_token_name(node->logical.type));
            log_ast_helper(f, node->logical.left, depth);
            log_ast_helper(f, node->logical.right, depth);
            break;
        case ast_type_background:
            fprintf(f, "%s:\n", get_token_name(token_bg));
            log_ast_helper(f, node->background.child, depth);
            break;
        case ast_type_pipe:
            fprintf(f, "%s:\n", get_token_name(token_pipe));
            log_ast_helper(f, node->pipe.left, depth);
            log_ast_helper(f, node->pipe.right, depth);
            break;
        case ast_type_list:
            log_list(f, node->list.children, depth);
            break;
    }
}

void log_ast(FILE *f, const ast_node *ast)
{
    fprintf(f, "LOG: AST:\n");
    log_ast_helper(f, ast, 0);
}
