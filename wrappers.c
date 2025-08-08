#include "wrappers.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    fflush(stderr);
    va_end(args);
}

int xfork()
{
    int pid;

    fflush(stderr);
    pid = fork();
    if (pid == -1) {
        log_error("fork: %s", strerror(errno));
        exit(13);
    }
    return pid;
}

void xpipe(int fd[2])
{
    int status;

    status = pipe(fd);
    if (status == -1) {
        log_error("pipe: %s", strerror(errno));
        exit(13);
    }
}

void xexecvp(const char *file, char *const argv[])
{
    execvp(file, argv);
    log_error("%s: %s", file, strerror(errno));
    _exit(13);
}
