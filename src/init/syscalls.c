// this file could probably just get generated by a script
#include <kernel/syscalls.h>

int _syscall(int, int, int, int);

_Noreturn void _syscall_exit(const char *msg, size_t len) {
	_syscall(_SYSCALL_EXIT, (int)msg, len, 0);
	__builtin_unreachable();
}

int _syscall_fork() {
	return _syscall(_SYSCALL_FORK, 0, 0, 0);
}

int _syscall_debuglog(const char *msg, size_t len) {
	return _syscall(_SYSCALL_DEBUGLOG, (int)msg, len, 0);
}
