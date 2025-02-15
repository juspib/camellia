#include <_proc.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	char *readbuf = malloc(4096);
	char *procbuf = malloc(4096);
	FILE *f = fopen("/proc/", "r");
	if (!f) {
		err(1, "couldn't open /proc/");
	}

	// TODO library for iterating over directories
	for (;;) {
		int len = fread(readbuf, 1, 4096, f);
		if (len <= 0) break;
		for (int pos = 0; pos < len; ) {
			char *end = memchr(readbuf + pos, 0, len - pos);
			if (!end) {
				errx(1, "unimplemented: buffer overflow");
			}
			size_t entryl = end - (readbuf+pos) + 1;
			if (isdigit(readbuf[pos])) {
				FILE *g;
				sprintf(procbuf, "/proc/%smem", readbuf+pos);
				g = fopen(procbuf, "r");
				if (!g) {
					warn("couldn't open \"%s\"", procbuf);
					strcpy(procbuf, "(can't peek)");
				} else {
					fseek(g, (long)&_psdata_loc->desc, SEEK_SET);
					if (fread(procbuf, 1, 128, g) <= 0) {
						strcpy(procbuf, "(no psdata)");
					}
					procbuf[128] = '\0';
					fclose(g);
				}
				end[-1] = '\0'; /* remove trailing slash */
				printf("%s\t%s\n", readbuf+pos, procbuf);
			}
			pos += entryl;
		}
	}

	free(readbuf);
	free(procbuf);
	fclose(f);
	return 0;
}
