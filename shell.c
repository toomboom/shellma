#include "shell.h"
#include "wrappers.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdio.h>


int have_sigint = 0;

static void set_signal(int s, void (*handler)(int))
{
    struct sigaction sa;

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(s, &sa, NULL);
}

static void sigchld_handler(int s)
{
    int pid;

    do {
        pid = waitpid(-1, NULL, WNOHANG);
    } while (pid > 0 || (pid == -1 && errno == EINTR));
}

static void sigint_handler(int s)
{
    have_sigint = 1; 
}

void enable_zombie_cleanup()
{
    set_signal(SIGCHLD, &sigchld_handler);
}

void disable_zombie_cleanup()
{
    set_signal(SIGCHLD, SIG_DFL);
}

void set_fg_pgroup(shell *sh, int pgrp)
{
    if (sh->tty_fd == -1 || sh->in_background) {
        return;
    }
    tcsetpgrp(sh->tty_fd, pgrp);
}

void restore_fg_pgroup(shell *sh)
{
    if (sh->tty_fd == -1 || sh->in_background) {
        return;
    }
    tcsetpgrp(sh->tty_fd, sh->pgid);
}

void reset_signals()
{
    set_signal(SIGTTOU, SIG_DFL);
    set_signal(SIGINT, SIG_DFL);
}

void init_shell(shell *sh)
{
    set_signal(SIGTTOU, SIG_IGN);
    set_signal(SIGINT, &sigint_handler);
    enable_zombie_cleanup();
    sh->tty_fd = isatty(0) ? 0 : -1;
    sh->pgid = getpgid(0);
    sh->last_status = 0;
    sh->in_background = sh->in_pipeline = 0;
}
