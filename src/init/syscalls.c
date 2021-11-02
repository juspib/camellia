// this file could probably just get generated by a script
#include <shared/syscalls.h>

_Noreturn void _syscall_exit(int ret) {
	_syscall(_SYSCALL_EXIT, ret, 0, 0, 0);
	__builtin_unreachable();
}

int _syscall_fork(void) {
	return _syscall(_SYSCALL_FORK, 0, 0, 0, 0);
}

int _syscall_await(void) {
	return _syscall(_SYSCALL_AWAIT, 0, 0, 0, 0);
}

handle_t _syscall_open(const char __user *path, int len) {
	return _syscall(_SYSCALL_OPEN, (int)path, len, 0, 0);
}

int _syscall_mount(handle_t handle, const char __user *path, int len) {
	return _syscall(_SYSCALL_MOUNT, handle, (int)path, len, 0);
}

int _syscall_read(handle_t handle, char __user *buf, int len, int offset) {
	return _syscall(_SYSCALL_READ, handle, (int)buf, len, offset);
}

int _syscall_write(handle_t handle, const char __user *buf, int len, int offset) {
	return _syscall(_SYSCALL_WRITE, handle, (int)buf, len, offset);
}

int _syscall_close(handle_t handle) {
	return _syscall(_SYSCALL_CLOSE, handle, 0, 0, 0);
}

handle_t _syscall_fs_create(void) {
	return _syscall(_SYSCALL_FS_CREATE, 0, 0, 0, 0);
}

int _syscall_fs_wait(char __user *buf, int max_len, struct fs_wait_response __user *res) {
	return _syscall(_SYSCALL_FS_WAIT, (int)buf, max_len, (int)res, 0);
}

int _syscall_fs_respond(char __user *buf, int ret) {
	return _syscall(_SYSCALL_FS_RESPOND, (int)buf, ret, 0, 0);
}

int _syscall_memflag(void __user *addr, size_t len, int flags) {
	return _syscall(_SYSCALL_MEMFLAG, (int)addr, len, flags, 0);
}
