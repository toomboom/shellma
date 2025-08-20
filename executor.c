#include "executor.h"
#include "wrappers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>


enum { pipe_read = 0, pipe_write = 1 };


/* TODO: maybe it be better way to use vector */
typedef struct wait_item_tag {
    int pid; 
    struct wait_item_tag *next;
} wait_item;

static void execute_ast_node(shell *sh, const ast_node *node);


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

static int get_exit_status(int status)
{
    return WIFEXITED(status)
        ? WEXITSTATUS(status)
        : 128 + WTERMSIG(status);
}

static int wait_pids(wait_item *head)
{
    int p, status = 0, last_cmd, result;

    last_cmd = head->pid;
    while (head != NULL) {
        p = xwait(&status);
        if (p == last_cmd) {
            result = get_exit_status(status);
        }
        remove_pid(&head, p);
    }
    return result;
}

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

builtin_fn find_builtin(const char *name)
{
    if (strcmp(name, "cd") == 0) {
        return &cd_builtin;
    }
    return NULL;
}

static void wait_for_pid(int pid, int *status, int options)
{
    int p;

    do {
        p = waitpid(-1, status, options);
    } while (p != pid);
}

static void execute_command(shell *sh, const ast_command *cmd)
{
    builtin_fn builtin_cb;
    int pid, status;

    builtin_cb = find_builtin(cmd->argv[0]);
    if (builtin_cb != NULL) {
        sh->last_status = builtin_cb(cmd->argv);
        if (sh->in_pipeline) {
            exit(sh->last_status);
        }
        return;
    }
    if (sh->in_pipeline) {
        xexecvp(cmd->argv[0], cmd->argv);
    }
    disable_zombie_cleanup();
    pid = xfork();
    if (pid == 0) {
        raise(SIGSTOP);
        reset_signals();
        xexecvp(cmd->argv[0], cmd->argv);
    }
    wait_for_pid(pid, NULL, WUNTRACED);
    if (sh->in_background) {
        xsetpgid(pid, shell->pgid);
    } else {
        xsetpgid(pid, pid);
        set_fg_pgroup(sh, pid);
    }
    kill(pid, SIGCONT);
    wait_for_pid(pid, &status, 0);
    restore_fg_pgroup(sh);
    enable_zombie_cleanup();
    sh->last_status = get_exit_status(status);
}

static void close_redir_files(redir_entry *entry, redir_entry *stop)
{
    while (entry != NULL && entry != stop) {
        xclose(entry->src_fd);
        entry = entry->next; 
    }
}

static void close_redir_target(redir_entry *entry)
{
    while (entry != NULL) {
        xclose(entry->target_fd);
        entry = entry->next; 
    }
}

static int open_redir_files(redir_entry *head)
{
    redir_entry *entry = head; 

    while (entry != NULL) {
        switch (entry->type) {
        case redir_in:
            entry->src_fd = xopen(entry->filename, O_RDONLY, 0666);
            break;
        case redir_out:
            entry->src_fd = xopen(entry->filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            break;
        case redir_append:
            entry->src_fd = xopen(entry->filename, O_WRONLY | O_CREAT | O_APPEND, 0666);
            break;
        }
        if (entry->src_fd == -1) {
            log_error("%s: %s", entry->filename, strerror(errno));
            close_redir_files(head, entry);
            return -1;
        }
        entry = entry->next; 
    }
    return 0;
}

static void replace_fd(int oldfd, int newfd)
{
    if (oldfd != newfd) {
        xdup2(oldfd, newfd);
        xclose(oldfd);
    }
}

static void execute_redirection(shell *sh, const ast_redirection *redir)
{
    redir_entry *entry = redir->entries;
    int status, i, orig_streams[3] = { -1, -1, -1 };

    status = open_redir_files(entry);
    if (status == -1) {
        sh->last_status = 1;
        return;
    }
    while (entry != NULL) {
        for (i = 0; i < 3; i++) {
            if (entry->target_fd == i) {
                if (orig_streams[i] == -1) {
                    orig_streams[i] = xdup(i);
                }
                break;
            }
        }
        replace_fd(entry->src_fd, entry->target_fd);
        entry = entry->next;
    }
    execute_ast_node(sh, redir->child);
    close_redir_target(redir->entries);
    for (i = 0; i < 3; i++) {
        if (orig_streams[i] != -1) {
            replace_fd(orig_streams[i], i);
        }
    }
}

typedef struct {
    int pgid;
    int next_read;
    wait_item *pids;
} pipeline_job;

static void redirect_and_exec(
    shell *sh, const ast_node *node,
    int read_fd, int write_fd)
{
    replace_fd(read_fd, 0);
    replace_fd(write_fd, 1);
    sh->in_pipeline = 1;
    reset_signals();
    execute_ast_node(sh, node);
}

static void pipeline_first(shell *sh, pipeline_job *job, const ast_node *node)
{
    int fd[2], pid;

    xpipe(fd);
    pid = xfork();
    if (pid == 0) {
        raise(SIGSTOP);
        xclose(fd[pipe_read]);
        redirect_and_exec(sh, node, 0, fd[pipe_write]);
    }
    wait_for_pid(pid, NULL, WUNTRACED);
    xsetpgid(pid, pid);
    set_fg_pgroup(sh, pid);
    kill(pid, SIGCONT);
    job->pgid = pid;
    xclose(fd[pipe_write]);
    job->next_read = fd[pipe_read];
    append_pid(&job->pids, pid);
}

static void pipeline_middle(shell *sh, pipeline_job *job, const ast_node *node)
{
    int fd[2], pid;

    xpipe(fd);
    pid = xfork();
    if (pid == 0) {
        xsetpgid(0, job->pgid);
        xclose(fd[pipe_read]);
        redirect_and_exec(sh, node, job->next_read, fd[pipe_write]);
    }
    xclose(fd[pipe_write]);
    xclose(job->next_read);
    job->next_read = fd[pipe_read];
    append_pid(&job->pids, pid);
}

static void pipeline_last(shell *sh, pipeline_job *job, const ast_node *node)
{
    int pid;

    pid = xfork();
    if (pid == 0) {
        xsetpgid(0, job->pgid);
        redirect_and_exec(sh, node, job->next_read, 1);
    }
    xclose(job->next_read);
    append_pid(&job->pids, pid);
}

static void execute_pipeline(shell *sh, const ast_pipeline *pipeline)
{
    pipeline_job job;
    ast_list_node *head = pipeline->chain; 
    
    job.pids = NULL;
    disable_zombie_cleanup();
    pipeline_first(sh, &job, head->node);
    head = head->next;
    while (head->next != NULL) {
        pipeline_middle(sh, &job, head->node);
        head = head->next;
    }
    pipeline_last(sh, &job, head->node);
    sh->last_status = wait_pids(job.pids);
    enable_zombie_cleanup();
    restore_fg_pgroup(sh);
}

static void execute_background(shell *sh, const ast_background *bg)
{
    int pid;

    pid = xfork();
    if (pid == 0) {
        reset_signals();
        xsetpgid(0, 0);
        sh->pgid = getpgid();
        sh->in_background = 1;
        execute_ast_node(sh, bg->child);
        _exit(sh->last_status);
    }
    sh->last_status = 0;
}

static void execute_logical(shell *sh, const ast_logical *logic)
{
    execute_ast_node(sh, logic->left);
    if ((sh->last_status == 0 && logic->type == token_and) ||
        (sh->last_status != 0 && logic->type == token_or))
    {
        execute_ast_node(sh, logic->right);
    }
}

static void execute_ast_node(shell *sh, const ast_node *node)
{
    switch (node->type) {
    case ast_type_command:
        execute_command(sh, &node->command);
        break;
    case ast_type_subshell:
        printf("Not implemented yet :(\n");
        if (sh->in_pipeline) {
            exit(0);
        }
        break;
    case ast_type_redirection:
        execute_redirection(sh, &node->redirection);
        break;
    case ast_type_pipeline:
        execute_pipeline(sh, &node->pipeline);
        break;
    case ast_type_logical:   
        execute_logical(sh, &node->logical);
        break;
    case ast_type_background:
        execute_background(sh, &node->background);
        break;
    }
}

void execute(shell *sh, const ast_list_node *stmts)
{
    sh->last_status = 0;
    while (stmts != NULL) {
        execute_ast_node(sh, stmts->node);
        stmts = stmts->next;
    }
}
