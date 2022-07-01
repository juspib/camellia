#include <init/stdlib.h>
#include <shared/printf.h>
#include <shared/syscalls.h>

libc_file __stdin  = {.fd = -1};
libc_file __stdout = {.fd = -1};

static void backend_file(void *arg, const char *buf, size_t len) {
	file_write((libc_file*)arg, buf, len);
}

int printf(const char *fmt, ...) {
	int ret = 0;
	va_list argp;
	va_start(argp, fmt);
	ret = __printf_internal(fmt, argp, backend_file, (void*)&__stdout);
	va_end(argp);
	return ret;
}

static void backend_buf(void *arg, const char *buf, size_t len) {
	char **ptrs = arg;
	size_t space = ptrs[1] - ptrs[0];
	if (len > space) len = space;
	memcpy(ptrs[0], buf, len);
	ptrs[0] += len;
}

int snprintf(char *str, size_t len, const char *fmt, ...) {
	int ret = 0;
	char *ptrs[2] = {str, str + len};
	va_list argp;
	va_start(argp, fmt);
	ret = __printf_internal(fmt, argp, backend_buf, &ptrs);
	va_end(argp);
	if (ptrs[0] < ptrs[1]) *ptrs[0] = '\0';
	return ret;
}

int _klogf(const char *fmt, ...) {
	// idiotic. however, this hack won't matter anyways
	char buf[256];
	int ret = 0;
	char *ptrs[2] = {buf, buf + sizeof buf};
	va_list argp;
	va_start(argp, fmt);
	ret = __printf_internal(fmt, argp, backend_buf, &ptrs);
	va_end(argp);
	if (ptrs[0] < ptrs[1]) *ptrs[0] = '\0';
	_syscall_debug_klog(buf, ret);
	return ret;
}


int file_open(libc_file *f, const char *path, int flags) {
	f->pos = 0;
	f->eof = false;
	f->fd = _syscall_open(path, strlen(path), flags);
	if (f->fd < 0) return f->fd;
	return 0;
}

int file_read(libc_file *f, char *buf, size_t len) {
	if (f->fd < 0) return -1;

	int res = _syscall_read(f->fd, buf, len, f->pos);
	if (res < 0) return res;
	if (res == 0 && len > 0) f->eof = true;
	f->pos += res;
	return res;
}

int file_write(libc_file *f, const char *buf, size_t len) {
	if (f->fd < 0) return -1;

	int res = _syscall_write(f->fd, buf, len, f->pos);
	if (res < 0) return res;
	f->pos += res;
	return res;
}

void file_close(libc_file *f) {
	if (f->fd > 0) _syscall_close(f->fd);
}
