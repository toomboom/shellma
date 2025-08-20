#ifndef WRAPPERS_SENTRY
#define WRAPPERS_SENTRY
#include <fcntl.h>


void log_error(const char *fmt, ...);
int xfork();
void xpipe(int fd[2]);
void xexecvp(const char *file, char *const argv[]);
int xwait(int *status);
int xwaitpid(int pid, int *status, int options);
int xdup(int oldfd);
void xdup2(int oldfd, int newfd);
int xclose(int fd);
int xopen(const char *path, int flags, mode_t mode);
void xsetpgid(int pid, int pgid);

#endif
