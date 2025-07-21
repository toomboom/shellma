#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


typedef struct token_item_tag {
    char *token;
    struct token_item_tag *next;
} token_item;

typedef struct token_list_tag {
    token_item *head, *tail;
} token_list;

static void token_list_init(token_list *list)
{
    list->head = list->tail = NULL;
}

static void token_list_free(token_list *list)
{
    while (list->head != NULL) {
        token_item *tmp = list->head;
        list->head = list->head->next;
        free(tmp->token);
        free(tmp);
    }
}

static void token_list_push(token_list *list, const char *token)
{
    token_item *tmp;

    tmp = malloc(sizeof(token_item));
    tmp->token = strdup(token);
    tmp->next = NULL;
    if (list->tail != NULL) {
        list->tail->next = tmp;
    } else {
        list->head = tmp;
    }
    list->tail = tmp;
}

static void token_list_traverse(
    const token_list *tl, void (*callback)(const token_item *, void *),
    void *userdata
){
    token_item *item = tl->head;

    while (item) {
        callback(item, userdata);
        item = item->next;
    }
}

typedef struct {
    int len, capacity;
    char *chars;
} strbuf;

static void strbuf_init(strbuf *str, int capacity)
{
    str->len = 0;
    str->capacity = capacity;
    str->chars = malloc(capacity);
}

static void strbuf_free(strbuf *str)
{
    str->len = str->capacity = 0;
    free(str->chars);
}

static void strbuf_append(strbuf *str, char ch)
{
    if (str->len >= str->capacity-1) {
        str->capacity *= 2;
        str->chars = realloc(str->chars, str->capacity);
    }
    str->chars[str->len] = ch;
    str->chars[str->len+1] = '\0';
    str->len++;
}

static void strbuf_clear(strbuf *str)
{
    str->len = 0; 
    str->chars[0] = '\0';
}

static int strbuf_readline(strbuf *str, FILE *f)
{
    int ch; 

    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        strbuf_append(str, ch);
    }
    return ch;
}

enum token_state { no_token, in_token, end_token };
enum parser_error { parsing_ok, unclosed_quote, unfinished_escape };
enum parser_state { processing, parser_eol, parser_eof, parser_err };

typedef struct {
    strbuf token;
    int in_squote, in_dquote, in_escape;
    enum token_state tstate;
    enum parser_state pstate;
    enum parser_error err;
    int line_num, char_num;
    FILE *file;
} token_parser;

static void clear_parser(token_parser *tp)
{
    strbuf_clear(&tp->token);
    tp->in_squote = tp->in_dquote = tp->in_escape = 0;
    tp->tstate = no_token;
    tp->pstate = processing;
    tp->err = parsing_ok;
    tp->char_num = 0;
}

static void token_parser_init(token_parser *tp, FILE *file)
{
    strbuf_init(&tp->token, 64);
    clear_parser(tp);
    tp->line_num = 0;
    tp->file = file;
}

static void token_parser_free(token_parser *tp)
{
    strbuf_free(&tp->token);
}

static void process_escaping(token_parser *tp, int ch)
{
    switch (ch) {
    case '\n':
        break;
    case EOF:
        tp->pstate = parser_eof;
        break;
    default:
        strbuf_append(&tp->token, ch);
    }
    tp->in_escape = 0;
}

/* todo: rename */
static void process_eol(token_parser *tp, int ch)
{
    if (tp->in_dquote || tp->in_squote) {
        tp->pstate = parser_err;
        tp->err = unclosed_quote;
        return;
    }
    if (tp->tstate == in_token) {
        tp->tstate = end_token;
    }
    tp->pstate = ch == EOF ? parser_eof : parser_eol;
}

static void process_quote(
    token_parser *tp, int ch, int *quote_flag, int in_other_quote
){
    if (in_other_quote) {
        strbuf_append(&tp->token, ch);
    } else {
        *quote_flag = !*quote_flag;
        tp->tstate = in_token;
    }
}

static void process_empty(token_parser *tp, int ch)
{
    if (tp->in_squote || tp->in_dquote) {
        strbuf_append(&tp->token, ch);
    }
    else if (tp->tstate == in_token) {
        tp->tstate = end_token;
    }
}

static void process_char(token_parser *tp, int ch)
{
    tp->char_num++;
    if (tp->in_escape) {
        process_escaping(tp, ch);
        return;
    }
    switch (ch) {
    case '\n': case EOF:
        process_eol(tp, ch);
        break;
    case ' ': case '\t':
        process_empty(tp, ch);
        break;
    case '\\':
        tp->in_escape = 1;
        break;
    case '"': 
        process_quote(tp, ch, &tp->in_dquote, tp->in_squote);
        break;
    case '\'':
        process_quote(tp, ch, &tp->in_squote, tp->in_dquote);
        break;
    default:
        tp->tstate = in_token;
        strbuf_append(&tp->token, ch);
    }
}

static void token_parser_tokenize(token_parser *tp, token_list *tl)
{
    int ch;

    clear_parser(tp);
    tp->line_num++;
    token_list_init(tl);
    do {
        ch = fgetc(tp->file);
        process_char(tp, ch);
        if (tp->tstate == end_token) {
            token_list_push(tl, tp->token.chars);
            tp->tstate = no_token;
            strbuf_clear(&tp->token);
        }
    } while (tp->pstate == processing);
    if (tp->pstate == parser_err) {
        token_list_free(tl);
    }
}

static void print_token(const token_item *item, void *userdata)
{
    printf("[%s]%c", item->token, item->next == NULL ? '\n' : ' ');
}

static void tokens_len(const token_item *item, void *userdata)
{
    int *sum = userdata;
    *sum += 1;
}

static void execute(const token_list *tl)
{
    int pid;
    char **argv;
    int len = 0, i; 
    token_item *item = tl->head;

    if (item == NULL) {
        return;
    }

    token_list_traverse(tl, &tokens_len, &len);
    argv = malloc(sizeof(char *) * (len + 1));
    for (i = 0; i < len; i++) {
        argv[i] = item->token;
        item = item->next;
    }
    argv[len] = NULL;
    fflush(stderr);
    pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        fflush(stderr);
        _exit(1);
    }
    free(argv);
    wait(NULL);
}

int main()
{
    token_parser tp;
    token_list tl;

    token_parser_init(&tp, stdin);
    for (;;) {
        printf("> ");
        token_parser_tokenize(&tp, &tl);
        if (tp.pstate == parser_err) {
            fprintf(stderr, "Error: line=%d, char=%d\n", tp.line_num, tp.char_num);
            continue;
        }
        execute(&tl);
        token_list_free(&tl);
        if (tp.pstate == parser_eof) {
            break;
        }
    }
    putchar('\n');
    token_parser_free(&tp);
    return 0;
}
