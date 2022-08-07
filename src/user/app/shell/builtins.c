#include "builtins.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <user/lib/fs/misc.h>

#define DEFAULT_ARGV(...) \
	char *_argv_default[] = {argv[0], __VA_ARGS__}; \
	if (argc <= 1) { \
		argc = sizeof(_argv_default) / sizeof(*_argv_default); \
		argv = _argv_default; \
	}

static void cmd_cat(int argc, char **argv) {
	const size_t buflen = 4096;
	char *buf = malloc(buflen);

	DEFAULT_ARGV("!stdin");
	for (int i = 1; i < argc; i++) {
		FILE *file = fopen(argv[i], "r");
		if (!file) {
			eprintf("couldn't open %s", argv[i]);
			return;
		}
		for (;;) {
			int len = fread(buf, 1, sizeof buf, file);
			if (len <= 0) break;
			fwrite(buf, 1, len, stdout);
		}
		fclose(file);
	}
}

static void cmd_echo(int argc, char **argv) {
	bool newline = true;
	int i = 1;

	if (!strcmp("-n", argv[i])) {
		i++;
		newline = false;
	}

	printf("%s", argv[i++]);
	for (; i < argc; i++)
		printf(" %s", argv[i]);
	if (newline)
		printf("\n");
}

void cmd_getsize(int argc, char **argv) {
	if (argc < 2) {
		eprintf("missing arguments");
		return;
	}

	for (int i = 1; i < argc; i++) {
		handle_t h = _syscall_open(argv[i], strlen(argv[i]), 0);
		if (h < 0) {
			eprintf("error opening %s", argv[i]);
			continue;
		}
		printf("%s: %d\n", argv[i], (int)_syscall_getsize(h));
		_syscall_close(h);
	}
}

void cmd_hexdump(int argc, char **argv) {
	const size_t buflen = 512;
	uint8_t *buf = malloc(buflen);
	FILE *file;
	int len;

	DEFAULT_ARGV("!stdin");
	for (int i = 1; i < argc; i++) {
		file = fopen(argv[i], "r");
		if (!file) {
			eprintf("couldn't open %s", argv[i]);
			return;
		}
		len = fread(buf, 1, buflen, file);
		fclose(file);

		for (int i = 0; i < len; i += 16) {
			printf("%08x  ", i);

			for (int j = i; j < i + 8 && j < len; j++)
				printf("%02x ", buf[j]);
			printf(" ");
			for (int j = i + 8; j < i + 16 && j < len; j++)
				printf("%02x ", buf[j]);
			printf(" |");

			for (int j = i; j < i + 16 && j < len; j++) {
				char c = '.';
				if (0x20 <= buf[j] && buf[j] < 0x7f) c = buf[j];
				printf("%c", c);
			}
			printf("|\n");
		}
	}
}

static void cmd_ls(int argc, char **argv) {
	FILE *file;
	const size_t buflen = 4096;
	char *buf = malloc(buflen);

	DEFAULT_ARGV("/");
	for (int i = 1; i < argc; i++) {
		char *path = (void*)argv[i];
		int pathlen = strlen(path);

		if (!pathlen || path[pathlen - 1] != '/') {
			memcpy(buf, path, pathlen);
			buf[pathlen] = '/';
			buf[pathlen+1] = '\0';
			path = buf;
		}

		file = fopen(path, "r");
		if (!file) {
			eprintf("couldn't open %s", argv[i]);
			return;
		}
		for (;;) {
			int len = fread(buf, 1, sizeof buf, file);
			if (len <= 0) break;
			for (int i = 0; i < len; i++)
				if (buf[i] == '\0') buf[i] = '\n';
			fwrite(buf, 1, len, stdout);
		}
		fclose(file);
	}
}

static void cmd_sleep(int argc, char **argv) {
	if (argc < 2) {
		eprintf("no arguments");
		return;
	}
	_syscall_sleep(strtol(argv[1], NULL, 0) * 1000);
}

static void cmd_touch(int argc, char **argv) {
	if (argc <= 1) {
		eprintf("no arguments");
		return;
	}

	for (int i = 1; i < argc; i++) {
		FILE *f = fopen(argv[i], "a");
		if (!f) eprintf("couldn't touch %s\n", argv[i]);
		fclose(f);
	}
}

static void cmd_whitelist(int argc, char **argv) {
	int split = 1;
	for (;;) {
		if (split >= argc) {
			eprintf("no command");
			return;
		}
		if (!strcmp("--", argv[split])) break;
		split++;
	}
	argv[split] = NULL;
	MOUNT_AT("/") { fs_whitelist((void*)&argv[1]); }
	run_args(argc - split - 1, &argv[split + 1], NULL);
}

struct builtin builtins[] = {
	{"cat", cmd_cat},
	{"echo", cmd_echo},
	{"getsize", cmd_getsize},
	{"hexdump", cmd_hexdump},
	{"ls", cmd_ls},
	{"sleep", cmd_sleep},
	{"touch", cmd_touch},
	{"whitelist", cmd_whitelist},
	{NULL, NULL},
};
