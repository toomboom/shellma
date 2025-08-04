#include "debug.h"
#include "strbuf.h"


void log_tokens(FILE *f, const token_item *token)
{
    fprintf(f, "LOG: TOKENS:\n");
    while (token != NULL) {
        fprintf(f, "([%s] %s)%c", token->value, get_token_name(token->type),
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

static void log_ast_list(FILE *f, const ast_list_node *head, int depth);

static void log_command(FILE *f, const ast_command *cmd)
{
    redir_entry *redirs = cmd->redirs;
    char **argv = cmd->argv;

    fprintf(f, "command: [");
    while (*argv != NULL) {
        fprintf(f, "%s%s", *argv, argv[1] == NULL ? "" : ", ");
        argv++;
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

static void log_ast_node(FILE *f, const ast_node *node, int depth)
{
    put_tabs(f, depth);
    if (node == NULL) {
        fprintf(f, "<empty>\n");
        return;
    }
    depth++;
    switch (node->type) {
    case ast_type_command:
        log_command(f, &node->command);
        break;
    case ast_type_subshell:
        fprintf(f, "subshell:\n");
        log_ast_list(f, node->subshell.statements, depth);
        break;
    case ast_type_pipeline:
        fprintf(f, "pipeline:\n");
        log_ast_list(f, node->pipeline.chain, depth);
        break;
    case ast_type_logical:
        fprintf(f, "%s:\n", get_token_name(node->logical.type));
        log_ast_node(f, node->logical.left, depth);
        log_ast_node(f, node->logical.right, depth);
        break;
    case ast_type_background:
        fprintf(f, "background:\n");
        log_ast_node(f, node->background.child, depth);
        break;
    }
}

static void log_ast_list(FILE *f, const ast_list_node *head, int depth)
{
    while (head != NULL) {
        log_ast_node(f, head->node, depth);
        head = head->next;
    }
}

void log_ast(FILE *f, const ast_list_node *list)
{
    fprintf(f, "LOG: AST:\n");
    log_ast_list(f, list, 0);
}
