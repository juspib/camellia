#include <kernel/arch/generic.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/virt.h>
#include <kernel/panic.h>
#include <kernel/proc.h>
#include <kernel/vfs/path.h>
#include <shared/flags.h>
#include <shared/syscalls.h>
#include <stdint.h>

_Noreturn void _syscall_exit(const char __user *msg, size_t len) {
	process_current->state = PS_DEAD;
	process_current->death_msg.buf = (userptr_t) msg; // discarding const
	process_current->death_msg.len = len;
	process_try2collect(process_current);
	process_switch_any();
}

int _syscall_await(char __user *buf, int len) {
	bool has_children = false;
	process_current->state = PS_WAITS4CHILDDEATH;
	process_current->death_msg.buf = buf;
	process_current->death_msg.len = len;

	// find any already dead children
	for (struct process *iter = process_current->child;
			iter; iter = iter->sibling) {
		if (iter->state == PS_DEAD) {
			int ret = process_try2collect(iter);
			assert(ret >= 0);
			return ret;
		}
		if (iter->state != PS_DEADER)
			has_children = true;
	}

	if (has_children)
		process_switch_any(); // wait until a child dies
	else {
		process_current->state = PS_RUNNING;
		return -1; // error
	}
}

int _syscall_fork(void) {
	struct process *child = process_fork(process_current);
	regs_savereturn(&child->regs, 0);
	return 1;
}

handle_t _syscall_open(const char __user *path, int len) {
	struct vfs_mount *mount;
	char *path_buf = NULL;

	if (PATH_MAX < len)
		return -1;
	if (process_find_handle(process_current) < 0)
		return -1;

	// copy the path to the kernel, simplify it
	path_buf = kmalloc(len); // gets freed in vfs_request_finish
	if (!virt_cpy_from(process_current->pages, path_buf, path, len))
		goto fail;

	len = path_simplify(path_buf, path_buf, len);
	if (len < 0) goto fail;

	mount = vfs_mount_resolve(process_current->mount, path_buf, len);
	if (!mount) goto fail;

	if (mount->prefix_len > 0) { // strip prefix
		len -= mount->prefix_len;
		// i can't just adjust path_buf, because it needs to be passed to free()
		// later on
		memcpy(path_buf, path_buf + mount->prefix_len, len);
	}

	return vfs_request_create((struct vfs_request) {
			.type = VFSOP_OPEN,
			.input = {
				.kern = true,
				.buf_kern = path_buf,
				.len = len,
			},
			.caller = process_current,
			.backend = mount->backend,
		});
fail:
	kfree(path_buf);
	return -1;
}

int _syscall_mount(handle_t handle, const char __user *path, int len) {
	struct vfs_mount *mount = NULL;
	struct vfs_backend *backend = NULL;
	char *path_buf = NULL;

	if (PATH_MAX < len) return -1;

	// copy the path to the kernel to simplify it
	path_buf = kmalloc(len);
	if (!virt_cpy_from(process_current->pages, path_buf, path, len))
		goto fail;
	len = path_simplify(path_buf, path_buf, len);
	if (len < 0) goto fail;

	// remove trailing slash
	// mounting something under `/this` and `/this/` should be equalivent
	if (path_buf[len - 1] == '/') {
		if (len == 0) goto fail;
		len--;
	}

	if (handle >= 0) { // mounting a real backend?
		if (handle >= HANDLE_MAX)
			goto fail;
		if (process_current->handles[handle].type != HANDLE_FS_FRONT)
			goto fail;
		backend = process_current->handles[handle].fs.backend;
	} // otherwise backend == NULL

	// append to mount list
	mount = kmalloc(sizeof *mount);
	mount->prev = process_current->mount;
	mount->prefix = path_buf;
	mount->prefix_len = len;
	mount->backend = backend;
	process_current->mount = mount;
	return 0;

fail:
	kfree(path_buf);
	kfree(mount);
	return -1;
}

int _syscall_read(handle_t handle_num, char __user *buf, int len, int offset) {
	struct handle *handle = &process_current->handles[handle_num];
	if (handle_num < 0 || handle_num >= HANDLE_MAX) return -1;
	if (handle->type != HANDLE_FILE) return -1;
	return vfs_request_create((struct vfs_request) {
			.type = VFSOP_READ,
			.output = {
				.buf = (userptr_t) buf,
				.len = len,
			},
			.id = handle->file.id,
			.offset = offset,
			.caller = process_current,
			.backend = handle->file.backend,
		});
}

int _syscall_write(handle_t handle_num, const char __user *buf, int len, int offset) {
	struct handle *handle = &process_current->handles[handle_num];
	if (handle_num < 0 || handle_num >= HANDLE_MAX) return -1;
	if (handle->type != HANDLE_FILE) return -1;
	return vfs_request_create((struct vfs_request) {
			.type = VFSOP_WRITE,
			.input = {
				.buf = (userptr_t) buf,
				.len = len,
			},
			.id = handle->file.id,
			.offset = offset,
			.caller = process_current,
			.backend = handle->file.backend,
		});
}

int _syscall_close(handle_t handle) {
	if (handle < 0 || handle >= HANDLE_MAX) return -1;
	return -1;
}

handle_t _syscall_fs_create(handle_t __user *back_user) {
	handle_t front, back = 0;
	struct vfs_backend *backend;

	front = process_find_handle(process_current);
	if (front < 0) goto fail;
	// the type needs to be set here so process_find_handle skips this handle
	process_current->handles[front].type = HANDLE_FS_FRONT;

	back  = process_find_handle(process_current);
	if (back < 0) goto fail;
	process_current->handles[back].type = HANDLE_FS_BACK;

	// copy the back handle to back_user
	if (!virt_cpy_to(process_current->pages, back_user, &back, sizeof(back)))
		goto fail;

	backend = kmalloc(sizeof *backend); // TODO never freed
	backend->type = VFS_BACK_USER;
	backend->handler = NULL;
	backend->queue = NULL;

	process_current->handles[front].fs.backend = backend;
	process_current->handles[back ].fs.backend = backend;

	return front;
fail:
	if (front >= 0)
		process_current->handles[front].type = HANDLE_EMPTY;
	if (back >= 0)
		process_current->handles[back].type = HANDLE_EMPTY;
	return -1;
}

int _syscall_fs_wait(handle_t back, char __user *buf, int max_len, 
		struct fs_wait_response __user *res) {
	struct handle *back_handle;

	if (back < 0 || back >= HANDLE_MAX) return -1;
	back_handle = &process_current->handles[back];
	if (back_handle->type != HANDLE_FS_BACK)
		return -1;

	process_current->state = PS_WAITS4REQUEST;
	back_handle->fs.backend->handler = process_current;
	/* checking the validity of those pointers here would make
	 * vfs_request_pass2handler simpler. TODO? */
	process_current->awaited_req.buf     = buf;
	process_current->awaited_req.max_len = max_len;
	process_current->awaited_req.res     = res;

	if (back_handle->fs.backend->queue) {
		// handle queued requests
		struct process *queued = back_handle->fs.backend->queue;
		back_handle->fs.backend->queue = NULL; // TODO get the next queued proc
		vfs_request_pass2handler(&queued->pending_req);
	} else {
		process_switch_any();
	}
}

int _syscall_fs_respond(char __user *buf, int ret) {
	struct vfs_request *req = process_current->handled_req;
	if (!req) return -1;

	if (req->output.len > 0 && ret > 0) {
		// if this vfsop outputs data and ret is positive, it's the length of the buffer
		// TODO document
		if (ret > req->output.len)
			ret = req->output.len; // i'm not handling this in a special way - the fs server can prevent this on its own
		if (!virt_cpy(req->caller->pages, req->output.buf,
					process_current->pages, buf, ret)) {
			// how should this error even be handled? TODO
		}
	}

	process_current->handled_req = NULL;
	vfs_request_finish(req, ret);
	return 0;
}

int _syscall_memflag(void __user *addr, size_t len, int flags) {
	userptr_t goal = addr + len;
	struct pagedir *pages = process_current->pages;
	if (flags != MEMFLAG_PRESENT) panic_unimplemented(); // TODO

	addr = (userptr_t)((int __force)addr & ~PAGE_MASK); // align to page boundary
	for (; addr < goal; addr += PAGE_SIZE) {
		if (pagedir_virt2phys(pages, addr, false, false)) {
			// allocated page, currently a no-op
			// if you'll be changing this - remember to check if the pages are owned by the kernel!
		} else {
			// allocate the new pages
			pagedir_map(pages, addr, page_alloc(1), true, true);
		}
	}

	return -1;
}

int _syscall(int num, int a, int b, int c, int d) {
	switch (num) {
		case _SYSCALL_EXIT:
			_syscall_exit((userptr_t)a, b);
		case _SYSCALL_AWAIT:
			return _syscall_await((userptr_t)a, b);
		case _SYSCALL_FORK:
			return _syscall_fork();
		case _SYSCALL_OPEN:
			return _syscall_open((userptr_t)a, b);
		case _SYSCALL_MOUNT:
			return _syscall_mount(a, (userptr_t)b, c);
		case _SYSCALL_READ:
			return _syscall_read(a, (userptr_t)b, c, d);
		case _SYSCALL_WRITE:
			return _syscall_write(a, (userptr_t)b, c, d);
		case _SYSCALL_CLOSE:
			return _syscall_close(a);
		case _SYSCALL_FS_CREATE:
			return _syscall_fs_create((userptr_t)a);
		case _SYSCALL_FS_WAIT:
			return _syscall_fs_wait(a, (userptr_t)b, c, (userptr_t)d);
		case _SYSCALL_FS_RESPOND:
			return _syscall_fs_respond((userptr_t)a, b);
		case _SYSCALL_MEMFLAG:
			return _syscall_memflag((userptr_t)a, b, c);
		default:
			tty_const("unknown syscall ");
			panic_unimplemented(); // TODO fail gracefully
	}
}
