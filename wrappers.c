#include "wrappers.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>


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

int xwait(int *status)
{
    int p;

    do {
        p = wait(status);
    } while (p == -1 && errno == EINTR);
    return p;
}

int xwaitpid(int pid, int *status, int options)
{
    int p;

    do {
        p = waitpid(pid, status, options);
    } while (p == -1 && errno == EINTR);
    return p;
}

int xdup(int oldfd)
{
    int fd;

    for (;;) {
        fd = dup(oldfd);
        if (fd != -1) {
            return fd;
        }
        if (errno != EINTR) {
            log_error("dup: %s", strerror(errno));
            exit(13);
        }
    }
}

void xdup2(int oldfd, int newfd)
{
    int fd;

    for (;;) {
        fd = dup2(oldfd, newfd);
        if (fd != -1) {
            return;
        }
        if (errno != EINTR) {
            log_error("dup2: %s", strerror(errno));
            exit(13);
        }
    }
}

int xclose(int fd)
{
    int status;

    status = close(fd);
    if (status == -1) {
        log_error("close: %s", strerror(errno));
    }
    return status;
}

int xopen(const char *path, int flags, mode_t mode)
{
    int status; 

    do {
        status = open(path, flags, mode);
    } while (status == -1 && errno == EINTR);
    return status;
}
