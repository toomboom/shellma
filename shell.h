#ifndef SHELL_SENTRY
#define SHELL_SENTRY


typedef struct {
    int last_status;
    int pgid;
    int tty_fd;
    int in_background, in_pipeline;
} shell;

extern int have_sigint;

void enable_zombie_cleanup();
void disable_zombie_cleanup();
void set_fg_pgroup(shell *sh, int pgrp);
void restore_fg_pgroup(shell *sh);
void init_shell(shell *sh);
void reset_signals();

#endif
