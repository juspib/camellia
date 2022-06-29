#include <init/malloc.h>
#include <shared/mem.h>
#include <shared/syscalls.h>
#include <stddef.h>

struct node {
	const char *name;
	size_t len;
	struct node *next;
};

struct node *root = NULL;

static struct node *lookup(const char *path, size_t len) {
	for (struct node *iter = root; iter; iter = iter->next) {
		if (iter->len == len && !memcmp(path, iter->name, len))
			return iter;
	}
	return NULL;
}

static int tmpfs_open(const char *path, struct fs_wait_response *res) {
	if (res->len == 0) return -1;
	path++;
	res->len--;

	if (res->len == 0) return 0; /* root */

	// no directory support (yet)
	if (memchr(path, '/', res->len)) return -1;

	if (res->flags & OPEN_CREATE) {
		if (lookup(path, res->len)) return -1; /* already exists */
		struct node *new = malloc(sizeof *new);
		char *namebuf = malloc(res->len);
		memcpy(namebuf, path, res->len);
		new->name = namebuf;
		new->len = res->len;
		new->next = root;
		root = new;
		return 1;
	}

	return lookup(path, res->len) != 0 ? 1 : -1;
}

void tmpfs_drv(void) {
	// TODO replace all the static allocations in drivers with mallocs
	static char buf[512];
	struct fs_wait_response res;
	while (!_syscall_fs_wait(buf, sizeof buf, &res)) {
		switch (res.op) {
			case VFSOP_OPEN:
				_syscall_fs_respond(NULL, tmpfs_open(buf, &res));
				break;

			case VFSOP_READ:
				if (res.id != 0) {
					// rw unimplemented
					_syscall_fs_respond(NULL, -1);
					break;
				}
				size_t buf_pos = 0;
				size_t to_skip = res.offset;

				for (struct node *iter = root; iter; iter = iter->next) {
					if (iter->len <= to_skip) {
						to_skip -= iter->len;
						continue;
					}

					if (iter->len + buf_pos - to_skip >= sizeof(buf)) {
						memcpy(buf + buf_pos, iter->name + to_skip, sizeof(buf) - buf_pos - to_skip);
						buf_pos = sizeof(buf);
						break;
					}
					memcpy(buf + buf_pos, iter->name + to_skip, iter->len - to_skip);
					buf_pos += iter->len - to_skip;
					buf[buf_pos++] = '\0';
					to_skip = 0;
				}
				_syscall_fs_respond(buf, buf_pos);
				break;

			default:
				_syscall_fs_respond(NULL, -1);
				break;
		}
	}

	_syscall_exit(1);
}
