#pragma once
#include <sys/types.h>

#define WIFSTOPPED(x) 0
#define WEXITSTATUS(x) ((x)&0xFF)
#define WIFEXITED(x) 1
#define WSTOPSIG(x) 0
#define WTERMSIG(x) 0

#define WNOHANG 0
#define WUNTRACED 0

pid_t wait3(int *wstatus, int opts, struct rusage *rusage);
