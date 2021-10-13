#include <init/shell.h>
#include <init/stdlib.h>
#include <init/tar.h>
#include <shared/flags.h>
#include <shared/syscalls.h>
#include <stdint.h>

#define argify(str) str, sizeof(str) - 1

extern char _bss_start; // provided by the linker
extern char _bss_end;
extern char _initrd;

void read_file(const char *path, size_t len);
void fs_prep(void);
void fs_test(void);
void test_await(void);

__attribute__((section(".text.startup")))
int main(void) {
	// allocate bss
	_syscall_memflag(&_bss_start, &_bss_end - &_bss_start, MEMFLAG_PRESENT);

	__tty_fd = _syscall_open("/tty", sizeof("/tty") - 1);
	if (__tty_fd < 0)
		_syscall_exit(1);

	fs_prep();
	fs_test();
	test_await();
	shell_loop();

	_syscall_exit(0);
}

void read_file(const char *path, size_t len) {
	int fd = _syscall_open(path, len);
	static char buf[64];
	int buf_len = 64;

	_syscall_write(__tty_fd, path, len, 0);
	printf(": ");
	if (fd < 0) {
		printf("couldn't open.\n");
		return;
	}

	buf_len = _syscall_read(fd, buf, buf_len, 0);
	_syscall_write(__tty_fd, buf, buf_len, 0);

	_syscall_close(fd);
}

void fs_prep(void) {
	handle_t front, back;
	front = _syscall_fs_create(&back);

	if (!_syscall_fork()) {
		tar_driver(back, &_initrd);
		_syscall_exit(1);
	}

	/* the trailing slash should be ignored by mount()
	 * TODO actually write tests */
	_syscall_mount(front, argify("/init/"));
}

void fs_test(void) {
	if (!_syscall_fork()) {
		/* run the "test" in a child process to not affect the fs view of the
		 * main process */
		read_file(argify("/init/fake.txt"));
		read_file(argify("/init/1.txt"));
		read_file(argify("/init/2.txt"));
		read_file(argify("/init/dir/3.txt"));

		printf("\nshadowing /init/dir...\n");
		_syscall_mount(-1, argify("/init/dir"));
		read_file(argify("/init/fake.txt"));
		read_file(argify("/init/1.txt"));
		read_file(argify("/init/2.txt"));
		read_file(argify("/init/dir/3.txt"));

		printf("\n");
		_syscall_exit(0);
	} else _syscall_await();
}

void test_await(void) {
	int ret;

	if (!_syscall_fork()) {
		/* this "test" runs in a child process, because otherwise it would be
		 * stuck waiting for e.g. the tar_driver process to exit */

		// regular exit()s
		if (!_syscall_fork()) _syscall_exit(69);
		if (!_syscall_fork()) _syscall_exit(420);

		// faults
		if (!_syscall_fork()) { // invalid memory access
			asm volatile("movb $69, 0" ::: "memory");
			printf("this shouldn't happen");
			_syscall_exit(-1);
		}
		if (!_syscall_fork()) { // #GP
			asm volatile("hlt" ::: "memory");
			printf("this shouldn't happen");
			_syscall_exit(-1);
		}

		while ((ret = _syscall_await()) != ~0)
			printf("await returned: %x\n", ret);
		printf("await: no more children\n");
	} else _syscall_await();
}
