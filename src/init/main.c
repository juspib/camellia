#include <init/tar.h>
#include <shared/flags.h>
#include <shared/syscalls.h>
#include <stdint.h>

#define argify(str) str, sizeof(str) - 1
#define log(str) _syscall_write(tty_fd, argify(str), 0)

extern char _bss_start; // provided by the linker
extern char _bss_end;
extern char _initrd;

int tty_fd;

void read_file(const char *path, size_t len);
void fs_test(void);
void test_await(void);

__attribute__((section(".text.startup")))
int main(void) {
	// allocate bss
	_syscall_memflag(&_bss_start, &_bss_end - &_bss_start, MEMFLAG_PRESENT);

	tty_fd = _syscall_open("/tty", sizeof("/tty") - 1);
	if (tty_fd < 0)
		_syscall_exit(argify("couldn't open tty"));

	fs_test();
	test_await();

	_syscall_exit(argify("my job here is done."));
}

void read_file(const char *path, size_t len) {
	int fd = _syscall_open(path, len);
	static char buf[64];
	int buf_len = 64;

	_syscall_write(tty_fd, path, len, 0);
	log(": ");
	if (fd < 0) {
		log("couldn't open.\n");
		return;
	}

	buf_len = _syscall_read(fd, buf, buf_len, 0);
	_syscall_write(tty_fd, buf, buf_len, 0);

	_syscall_close(fd);
}

void fs_test(void) {
	handle_t front, back;
	front = _syscall_fs_create(&back);

	if (_syscall_fork()) {
		// child: is the fs server
		tar_driver(back, &_initrd);
		return;
	}

	// parent: accesses the fs
	log("\n\n");
	// the trailing slash should be ignored by mount()
	_syscall_mount(front, argify("/init/"));
	read_file(argify("/init/fake.txt"));
	read_file(argify("/init/1.txt"));
	read_file(argify("/init/2.txt"));
	read_file(argify("/init/dir/3.txt"));

	log("\nshadowing /init/dir...\n");
	_syscall_mount(-1, argify("/init/dir"));
	read_file(argify("/init/fake.txt"));
	read_file(argify("/init/1.txt"));
	read_file(argify("/init/2.txt"));
	read_file(argify("/init/dir/3.txt"));

	log("\n");
}

void test_await(void) {
	char buf[16];
	int len;

	// the child immediately dies
	if (!_syscall_fork())
		_syscall_exit(argify("i'm dead"));
	if (!_syscall_fork())
		_syscall_exit(argify("i'm also dead"));

	while ((len = _syscall_await(buf, 16)) >= 0) {
		log("await returned: ");
		_syscall_write(tty_fd, buf, len, 0);
		log("\n");
	}
	log("await: negative len\n");
}
