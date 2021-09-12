// this file could probably just get generated by a script
#include <init/types.h>
#include <shared/syscalls.h>

int _syscall(int, int, int, int);

_Noreturn void _syscall_exit(const char __user *msg, size_t len) {
	_syscall(_SYSCALL_EXIT, (int)msg, len, 0);
	__builtin_unreachable();
}

int _syscall_fork(void) {
	return _syscall(_SYSCALL_FORK, 0, 0, 0);
}

int _syscall_await(char __user *buf, int len) {
	return _syscall(_SYSCALL_AWAIT, (int)buf, (int)len, 0);
}

handle_t _syscall_open(const char __user *path, int len) {
	return _syscall(_SYSCALL_OPEN, (int)path, len, 0);
}

int _syscall_mount(handle_t handle, const char __user *path, int len) {
	return _syscall(_SYSCALL_MOUNT, handle, (int)path, len);
}

int _syscall_read(handle_t handle, char __user *buf, int len) {
	return _syscall(_SYSCALL_READ, handle, (int)buf, len);
}

int _syscall_write(handle_t handle, const char __user *buf, int len) {
	return _syscall(_SYSCALL_WRITE, handle, (int)buf, len);
}

int _syscall_close(handle_t handle) {
	return _syscall(_SYSCALL_CLOSE, handle, 0, 0);
}

handle_t _syscall_fs_create(handle_t __user *back) {
	return _syscall(_SYSCALL_FS_CREATE, (int)back, 0, 0);
}

int _syscall_fs_wait(handle_t back, char __user *buf, int __user *len) {
	return _syscall(_SYSCALL_FS_WAIT, back, (int)buf, (int)len);
}

int _syscall_fs_respond(char __user *buf, int ret) {
	return _syscall(_SYSCALL_FS_RESPOND, (int)buf, ret, 0);
}

int _syscall_memflag(void __user *addr, size_t len, int flags) {
	return _syscall(_SYSCALL_MEMFLAG, (int)addr, len, flags);
}
