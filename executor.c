#include "executor.h"
#include "wrappers.h"
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>


enum exec_behavior { exec_return, exec_replace, exec_exit };
typedef struct {
    enum exec_behavior behavior;
    int last_status;
} executor;

static void execute_ast_node(executor *e, const ast_node *node);

static void set_signal(int s, void (*handler)(int), sigset_t *mask, int flags)
{
    struct sigaction sa;

    sa.sa_handler = handler;
    if (mask != NULL) {
        memcpy(&sa.sa_mask, mask, sizeof(sigset_t));
    } else {
        sigemptyset(&sa.sa_mask);
    }
    sa.sa_flags = flags;
    sigaction(s, &sa, NULL);
}

static void sigchld_handler(int s)
{
    int pid;

    do {
        pid = waitpid(-1, NULL, WNOHANG);
    } while (pid > 0);
}

static void cleanup_zombies(int flag)
{
    if (flag) {
        set_signal(SIGCHLD, &sigchld_handler, NULL, SA_NOCLDSTOP);
    } else {
        set_signal(SIGCHLD, SIG_DFL, NULL, 0);
    }
}

static void pipeline_exec_command(
    executor *e,
    const ast_node *node, int read_fd, int write_fd)
{
    if (read_fd != 0) {
        dup2(read_fd, 0);
        close(read_fd);
    }
    if (write_fd != 1) {
        dup2(write_fd, 1);
        close(write_fd);
    }
    e->behavior = exec_replace;
    execute_ast_node(e, node);
}

typedef struct wait_item_tag {
    int pid; 
    struct wait_item_tag *next;
} wait_item;

static void append_pid(wait_item **phead, int pid)
{
    wait_item *tmp;

    tmp = malloc(sizeof(wait_item));
    tmp->pid = pid;
    tmp->next = *phead;
    *phead = tmp;
}

static void remove_pid(wait_item **phead, int pid)
{
    while (*phead != NULL) {
        if ((*phead)->pid == pid) {
            wait_item *tmp = *phead;

            *phead = (*phead)->next;
            free(tmp);
        } else {
            phead = &(*phead)->next;
        }
    }
}

static int wait_pids(wait_item *head)
{
    int p, status = 0;

    while (head != NULL) {
        p = wait(&status);
        remove_pid(&head, p);
    }
    return status;
}

static void execute_pipeline(executor *e, const ast_pipeline *pipeline)
{
    enum { pipe_read = 0, pipe_write = 1 };
    ast_list_node *head = pipeline->chain; 
    wait_item *pids = NULL;
    int pid, fd[2], next_read = 0;
    
    cleanup_zombies(0);
    while (head->next != NULL) {
        xpipe(fd);
        pid = xfork();
        append_pid(&pids, pid);
        if (pid == 0) {
            close(fd[pipe_read]);
            pipeline_exec_command(e, head->node, next_read, fd[pipe_write]);
        }
        if (next_read != 0) {
            close(next_read);
        }
        next_read = fd[pipe_read];
        close(fd[pipe_write]);
        head = head->next;
    }
    pid = xfork();
    append_pid(&pids, pid);
    if (pid == 0) {
        pipeline_exec_command(e, head->node, next_read, 1);
    }
    close(next_read);
    e->last_status = wait_pids(pids);
    cleanup_zombies(1);
}

static void execute_background(executor *e, const ast_background *bg)
{
    int pid;

    pid = xfork();
    if (pid == 0) {
        e->behavior = exec_exit;
        execute_ast_node(e, bg->child);
    }
    e->last_status = 0;
}

static int redirect_open(const redir_entry *redir)
{
    switch (redir->type) {
    case token_redir_out:
        return open(redir->filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    case token_redir_append:
        return open(redir->filename, O_WRONLY|O_CREAT|O_APPEND, 0666);
    case token_redir_in:
        return open(redir->filename, O_RDONLY);
    default:
        /* todo: add enum for redirect */
        log_error("Unreachable code");
        errno = EINVAL;
        return -1;
    }
}

static int apply_redirections(const redir_entry *redirs)
{
    int fd;

    while (redirs != NULL) {
        fd = redirect_open(redirs);
        if (fd == -1) {
            log_error("%s: %s", redirs->filename, strerror(errno));
            return -1;
        }
        dup2(fd, redirs->fd);
        close(fd);
        redirs = redirs->next;
    }
    return 0;
}

#if 0
static int cd_builtin(char **argv)
{
    int status;
    const char *path;

    if (argv[1] == NULL) {
        path = getenv("HOME");
        if (path == NULL) {
            log_error("HOME variable is not set");
            return 1;
        }
    } else {
        path = argv[1];
    }

    status = chdir(path);
    if (status == -1) {
        log_error("cd: %s: %s", path, strerror(errno));
        return 1;
    }
    return 0;
}

typedef int (*builtin_fn)(char **argv);

builtin_fn find_builtin_fn(const char *name)
{
    /* todo: add echo pwd exit etc */
    if (strcmp(name, "cd") == 0) {
        return &cd_builtin;
    }
    return NULL;
}
#endif

static void execute_command(executor *e, const ast_command *cmd)
{
    int pid, p, status;

    cleanup_zombies(0);
    pid = e->behavior == exec_replace ? 0 : xfork();
    if (pid == 0) {
        status = apply_redirections(cmd->redirs);
        if (status == -1) {
            _exit(1);
        }
        xexecvp(cmd->argv[0], cmd->argv);
    }
    do {
        p = wait(&status);
    } while (p != pid);
    cleanup_zombies(1);
    e->last_status = WIFEXITED(status)
        ? WEXITSTATUS(status)
        : 128 + WTERMSIG(status);
}

static void execute_logical(executor *e, const ast_logical *logic)
{
    execute_ast_node(e, logic->left);
    if ((e->last_status == 0 && logic->type == token_and) ||
        (e->last_status != 0 && logic->type == token_or))
    {
        execute_ast_node(e, logic->right);
    }
}

static void execute_ast_node(executor *e, const ast_node *node)
{
    switch (node->type) {
    case ast_type_command:
        execute_command(e, &node->command);
        break;
    case ast_type_subshell:
        printf("Not implemented yet :(\n");
        break;
    case ast_type_pipeline:
        execute_pipeline(e, &node->pipeline);
        break;
    case ast_type_logical:   
        execute_logical(e, &node->logical);
        break;
    case ast_type_background:
        execute_background(e, &node->background);
        break;
    }
    if (e->behavior == exec_exit) {
        _exit(e->last_status);
    }
}

int execute(const ast_list_node *stmts)
{
    executor e;

    cleanup_zombies(1);
    e.last_status = 0;
    e.behavior = exec_return;
    while (stmts != NULL) {
        execute_ast_node(&e, stmts->node);
        stmts = stmts->next;
    }
    return e.last_status;
}
