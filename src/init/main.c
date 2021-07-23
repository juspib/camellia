#include <kernel/syscalls.h>
#include <stddef.h>
#include <stdint.h>

int _syscall(int, int, int, int);

void exit(const char *msg, size_t len) {
	_syscall(SC_EXIT, (void*)msg, len, 0);
	__builtin_unreachable();
}

int debuglog(const char *msg, size_t len) {
	return _syscall(SC_DEBUGLOG, (void*)msg, len, 0);
}


int main() {
	debuglog("hello from init! ",
	  sizeof("hello from init! ") - 1);

	exit(    "bye from init! ",
	  sizeof("bye from init! ") - 1);
}
