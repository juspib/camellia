#include <init/fs/misc.h>
#include <init/shell.h>
#include <init/stdlib.h>
#include <init/tar.h>
#include <init/tests/main.h>
#include <shared/flags.h>
#include <shared/syscalls.h>
#include <stdint.h>

// TODO move to shared header file
#define argify(str) str, sizeof(str) - 1

extern char _bss_start; // provided by the linker
extern char _bss_end;
extern char _initrd;

void read_file(const char *path, size_t len);
void fs_prep(void);

__attribute__((section(".text.startup")))
int main(void) {
	// allocate bss
	_syscall_memflag(&_bss_start, &_bss_end - &_bss_start, MEMFLAG_PRESENT);

	__tty_fd = _syscall_open("/tty", sizeof("/tty") - 1);
	if (__tty_fd < 0)
		_syscall_exit(1);

	// change screen color
	handle_t vga = _syscall_open(argify("/vga"));
	for (int i = 1; i < 80*25*2; i += 2)
		_syscall_write(vga, "\x4f", 1, i);
	_syscall_close(vga);

	fs_prep();
	shell_loop();

	_syscall_exit(0);
}

void fs_prep(void) {
	handle_t front = _syscall_fs_fork2();
	if (!front) {
		tar_driver(&_initrd);
		_syscall_exit(1);
	}

	/* the trailing slash should be ignored by mount()
	 * TODO actually write tests */
	_syscall_mount(front, argify("/init/"));

	if (!fork2_n_mount("/")) fs_dir_inject("/init/");

	/* from here on i'll just use the helper MOUNT macro */

	/* passthrough fs */
	MOUNT("/2nd/", fs_passthru(NULL));     /* copies / under /2nd */
	MOUNT("/3rd/", fs_passthru("/init/")); /* copies /init under /3rd */
}
