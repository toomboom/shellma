#ifndef WRAPPERS_SENTRY
#define WRAPPERS_SENTRY


void log_error(const char *fmt, ...);
int xfork();
void xpipe(int fd[2]);
void xexecvp(const char *file, char *const argv[]);

#endif
