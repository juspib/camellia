/* generated by syscall.c.awk
 * don't modify manually, instead run:
 *     make src/user/lib/syscall.c
 */
#include <shared/syscalls.h>


_Noreturn void _syscall_exit(long ret) {
	_syscall(_SYSCALL_EXIT, ret, 0, 0, 0);
	__builtin_unreachable();
}

long _syscall_await(void) {
	return _syscall(_SYSCALL_AWAIT, 0, 0, 0, 0);
}

long _syscall_fork(int flags, handle_t __user *fs_front) {
	return _syscall(_SYSCALL_FORK, (long)flags, (long)fs_front, 0, 0);
}

handle_t _syscall_open(const char __user *path, long len, int flags) {
	return (handle_t)_syscall(_SYSCALL_OPEN, (long)path, len, (long)flags, 0);
}

long _syscall_mount(handle_t h, const char __user *path, long len) {
	return _syscall(_SYSCALL_MOUNT, (long)h, (long)path, len, 0);
}

handle_t _syscall_dup(handle_t from, handle_t to, int flags) {
	return (handle_t)_syscall(_SYSCALL_DUP, (long)from, (long)to, (long)flags, 0);
}

long _syscall_read(handle_t h, void __user *buf, size_t len, long offset) {
	return _syscall(_SYSCALL_READ, (long)h, (long)buf, (long)len, offset);
}

long _syscall_write(handle_t h, const void __user *buf, size_t len, long offset) {
	return _syscall(_SYSCALL_WRITE, (long)h, (long)buf, (long)len, offset);
}

long _syscall_close(handle_t h) {
	return _syscall(_SYSCALL_CLOSE, (long)h, 0, 0, 0);
}

long _syscall_fs_wait(char __user *buf, long max_len, struct fs_wait_response __user *res) {
	return _syscall(_SYSCALL_FS_WAIT, (long)buf, max_len, (long)res, 0);
}

long _syscall_fs_respond(void __user *buf, long ret, int flags) {
	return _syscall(_SYSCALL_FS_RESPOND, (long)buf, ret, (long)flags, 0);
}

void __user *_syscall_memflag(void __user *addr, size_t len, int flags) {
	return (void __user *)_syscall(_SYSCALL_MEMFLAG, (long)addr, (long)len, (long)flags, 0);
}

long _syscall_pipe(handle_t __user user_ends[2], int flags) {
	return _syscall(_SYSCALL_PIPE, (long)user_ends, (long)flags, 0, 0);
}

long _syscall_execbuf(void __user *buf, size_t len) {
	return _syscall(_SYSCALL_EXECBUF, (long)buf, (long)len, 0, 0);
}

void _syscall_debug_klog(const void __user *buf, size_t len) {
	return (void)_syscall(_SYSCALL_DEBUG_KLOG, (long)buf, (long)len, 0, 0);
}

