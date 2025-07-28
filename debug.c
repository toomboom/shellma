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

static void log_command(FILE *f, char * const *argv)
{
    int i;

    fprintf(f, "command: [");
    for (i = 0; argv[i] != NULL; i++) {
        fprintf(f, "%s%s", argv[i], argv[i+1] == NULL ? "" : ", ");
    }
    fprintf(f, "]\n");
}

static void log_ast_helper(FILE *f, const ast_node *node, int depth);

static void log_sequence(FILE *f, ast_node **children, int len, int depth)
{
    int i;
    
    fprintf(f, "sequence[%d]:\n", len);
    for (i = 0; i < len; i++) {
        log_ast_helper(f, children[i], depth);
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
            log_command(f, node->command.argv);
            break;
        case ast_type_redirect:
            fprintf(f, "%s %s:\n", get_token_name(node->redirect.type),
                    node->redirect.filename);
            log_ast_helper(f, node->redirect.child, depth);
            break;
        case ast_type_logical:
            fprintf(f, "%s:\n", get_token_name(node->logical.operator));
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
        case ast_type_sequence:
            log_sequence(f, node->sequence.children, node->sequence.len, depth);
            break;
    }
}

void log_ast(FILE *f, const ast_node *ast)
{
    fprintf(f, "LOG: AST:\n");
    log_ast_helper(f, ast, 0);
}

#if 0
static void log_command(FILE *f, char **argv)
{
    putchar('"');
    while (*argv != NULL) {
        printf("[%s]%s", *argv, argv[1] == NULL ? "" : " ");
        argv++;
    }
    putchar('"');
}

void log_node(FILE *f, const ast_node *node, int depth)
{
    if (depth > 1) {
        int i;

        fputc(f, '|');
        for (i = 0; i < depth + 1; i++) {
            fputc(f, '-');
        }
    }
    switch (node->type) {
    case ast_type_command:
        log_command(node->command.argv);
        break;
    case ast_type_logical:   
    case ast_type_background:     
    case ast_type_pipe:
    }
}

void log_ast_helper(FILE *f, const ast_node *tree, int depth)
{
    if (tree == NULL) {
        return;
    }
    depth++;
    if (depth > 1) {
        fputc(f, '|');
        
    }
    switch (tree->type) {
    case ast_type_command:
        print_spaces(spaces);
        print_command_node(tree);
        break;
    case ast_type_pipe:
        print_ast(tree->pipe.right, spaces);
        print_spaces(spaces);
        printf("%s\n", get_operator(token_pipe));
        print_ast(tree->pipe.left, spaces);
        break;
    case ast_type_logical:
        print_ast(tree->logical.right, spaces);
        print_spaces(spaces);
        printf("%s\n", get_operator(tree->logical.operator));
        print_ast(tree->logical.left, spaces);
        break;
    case ast_type_background:
        print_spaces(spaces);
        printf("& -> ");
        print_ast(tree->background.child, spaces);
        break;
    }
    putchar('\n');
}

void log_ast(FILE *file, const ast_node *tree)
{
    log_ast_helper(file, tree, 0);
}

static void init_cmd(strbuf *str, char *const *argv)
{
    int i;

    strbuf_init(str, 64);
    strbuf_append(str, '[');
    for (i = 0; argv[i] != NULL; i++) {
        strbuf_append(str, '\'');
        strbuf_join(str, argv[i]);
        strbuf_append(str, '\'');
        if (argv[i+1] != NULL) {
            strbuf_append(str, ' ');
        }
    }
    strbuf_append(str, ']');
}

static void log_ast_helper(FILE *f, const ast_node *ast, int depth);

static void log_node(
    FILE *f, const ast_node *right, const ast_node *left,
    const char *info, int depth)
{
    log_ast_helper(f, left, depth + 1);
    put_spaces(f, depth);
    fprintf(f, "%s\n", info);
    log_ast_helper(f, right, depth + 1);
}
static void log_ast_helper(FILE *f, const ast_node *ast, int depth)
{
    strbuf cmd;

    if (ast == NULL) {
        fputc('\n', f);
        return;
    }
    switch (ast->type) {
    case ast_type_command:
        init_cmd(&cmd, ast->command.argv);
        log_node(f, NULL, NULL, cmd.chars, depth);
        strbuf_free(&cmd);
        break;
    case ast_type_pipe:
        log_node(f, ast->pipe.right, ast->pipe.left,
                 get_token_name(token_pipe), depth);
        break;
    case ast_type_logical:
        log_node(f, ast->logical.right, ast->logical.left,
                 get_token_name(ast->logical.operator), depth);
        break;
    case ast_type_background:
        log_node(f, NULL, ast->background.child, 
                 get_token_name(token_bg), depth);
        break;
    }
}
const char* token_type_to_str(enum token_type op) {
    switch (op) {
        case TOKEN_AND: return "&&";
        case TOKEN_OR:  return "||";
        default:        return "UNKNOWN_OP";
    }
}


#endif
