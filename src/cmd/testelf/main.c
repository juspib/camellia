#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *str = "Hello!", *str2 = "World.";

int main(int argc, char **argv) {
	printf("elftest's &main == %p\n", &main);
	printf("%s %s\n", str, str2);
	printf("argc == %u\n", argc);
	for (int i = 0; i < argc; i++)
		printf("argv[%u] == %p == \"%s\"\n", i, argv[i], argv[i]);
	if (argv[1] && strcmp(argv[1], "stackexec") == 0) {
		/* exec something with arguments on the stack */
		const char s_d[] = "I am a pretty long string on the stack. Oh my. " \
			"I hope I won't get corrupted.\0";
		char s[sizeof(s_d)];
		memcpy(s, s_d, sizeof(s_d));
		const char *argv2[] = {"/bin/testelf", s, s, "hello", s, s, s, "lol", NULL};
		printf("argv2 == %p, s == %p\n== exec ==\n", argv2, s);
		execv((void*)argv2[0], (void*)argv2);
		err(1, "execv");
	}
	return 0;
}
