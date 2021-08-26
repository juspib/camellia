#include <kernel/mem.h>
#include <kernel/util.h>
#include <kernel/vfs/mount.h>

struct vfs_mount *vfs_mount_seed(void) {
	struct vfs_mount *mount = kmalloc(sizeof(struct vfs_mount));
	*mount = (struct vfs_mount){
		.prev = NULL,
		.prefix = "/tty",
		.prefix_len = 4,
		.fd = {
			.type = FD_SPECIAL_TTY,
		},
	};
	return mount;
}

struct vfs_mount *vfs_mount_resolve(
		struct vfs_mount *top, const char *path, size_t path_len)
{
	for (; top; top = top->prev) {
		if (top->prefix_len > path_len)
			continue;
		if (memcmp(top->prefix, path, top->prefix_len) == 0)
			break;
	}
	return top;
}
