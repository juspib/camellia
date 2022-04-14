#include <kernel/arch/generic.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/virt.h>
#include <kernel/panic.h>
#include <kernel/proc.h>
#include <kernel/vfs/mount.h>
#include <shared/mem.h>
#include <stdint.h>
#include <kernel/vfs/root.h> // TODO

struct process *process_first;
struct process *process_current;
static uint32_t next_pid = 0;

struct process *process_seed(void) {
	struct process *proc = kmalloc(sizeof *proc);
	proc->pages = pagedir_new();
	proc->state = PS_RUNNING;
	proc->sibling = NULL;
	proc->child   = NULL;
	proc->parent  = NULL;
	proc->mount   = vfs_mount_seed();
	proc->id      = next_pid++;
	proc->handled_req = NULL;
	proc->controlled  = NULL;

	process_first = proc;

	for (int i = 0; i < HANDLE_MAX; i++)
		proc->handles[i].type = HANDLE_EMPTY;

	// map the stack to the last page in memory
	pagedir_map(proc->pages, (userptr_t)~PAGE_MASK, page_alloc(1), true, true);
	proc->regs.esp = (userptr_t) ~0xF;

	// map the kernel
	//   yup, .text is writeable too. the plan is to not map the kernel
	//   into user memory at all, but i'll implement that later. TODO
	for (size_t p = 0x100000; p < (size_t)&_bss_end; p += PAGE_SIZE)
		pagedir_map(proc->pages, (userptr_t)p, (void*)p, false, true);

	// the kernel still has to load the executable code and set EIP
	return proc;
}

struct process *process_fork(struct process *parent) {
	struct process *child = kmalloc(sizeof *child);
	memcpy(child, parent, sizeof *child);

	child->pages = pagedir_copy(parent->pages);
	child->sibling = parent->child;
	child->child   = NULL;
	child->parent  = parent;
	parent->child  = child;

	parent->handled_req = NULL; // TODO control this with a flag

	child->id      = next_pid++;

	return child;
}

void process_free(struct process *p) {
	assert(p->state == PS_DEADER);
	pagedir_free(p->pages);
	if (p->child) { // TODO
		// panic_invalid_state();
		return;
	}
	if (p->parent && p->parent->child == p) {
		p->parent->child = p->sibling;
	} else {
		// this would be simpler if siblings were a doubly linked list
		struct process *prev = p->parent->child;
		while (prev->sibling != p) {
			prev = prev->sibling;
			assert(prev);
		}
		prev->sibling = p->sibling;
	}
	kfree(p);
}

void process_switch(struct process *proc) {
	assert(proc->state == PS_RUNNING);
	process_current = proc;
	pagedir_switch(proc->pages);
	sysexit(proc->regs);
}

_Noreturn void process_switch_any(void) {
	struct process *found = process_find(PS_RUNNING);
	if (found) process_switch(found);
	process_idle();
}

_Noreturn void process_idle(void) {
	struct process *procs[16];
	size_t len = process_find_multiple(PS_WAITS4IRQ, procs, 16);

	if (len == 0) {
		mem_debugprint();
		cpu_shutdown();
	}

	for (;;) {
		for (size_t i = 0; i < len; i++) {
			if (procs[i]->waits4irq.ready()) {
				/* if this is entered during the first iteration, it indicates a
				 * kernel bug. this should be logged. TODO? */
				vfs_root_handler(&procs[i]->waits4irq.req); // TODO this should be a function pointer too
				process_switch_any();
			}
		}
		cpu_pause();
	}
}

struct process *process_next(struct process *p) {
	/* is a weird depth-first search, the search order is:
	 *         1
	 *        / \
	 *       2   5
	 *      /|   |\
	 *     3 4   6 7
	 */
	if (!p)	return NULL;
	if (p->child)	return p->child;
	if (p->sibling)	return p->sibling;

	/* looking at the diagram above - we're at 4, want to find 5 */
	while (!p->sibling) {
		p = p->parent;
		if (!p) return NULL;
	}
	return p->sibling;
}

struct process *process_find(enum process_state target) {
	struct process *result = NULL;
	process_find_multiple(target, &result, 1);
	return result;
}

size_t process_find_multiple(enum process_state target, struct process **buf, size_t max) {
	size_t i = 0;
	for (struct process *p = process_first;
		i < max && p;
		p = process_next(p))
	{
		if (p->state == target) buf[i++] = p;
	}
	return i;
}

handle_t process_find_handle(struct process *proc) {
	handle_t handle;
	for (handle = 0; handle < HANDLE_MAX; handle++) {
		if (proc->handles[handle].type == HANDLE_EMPTY)
			break;
	}
	if (handle == HANDLE_MAX) handle = -1;
	return handle;
}

void process_kill(struct process *proc, int ret) {
	// TODO kill children
	proc->state = PS_DEAD;
	proc->death_msg = ret;
	process_try2collect(proc);
	if (proc == process_first) {
		kprintf("init killed, quitting...");
		mem_debugprint();
		cpu_shutdown();
	}
}

int process_try2collect(struct process *dead) {
	struct process *parent = dead->parent;
	int ret;

	assert(dead->state == PS_DEAD);

	switch (parent->state) {
		case PS_WAITS4CHILDDEATH:
			ret = dead->death_msg;
			regs_savereturn(&parent->regs, ret);
			parent->state = PS_RUNNING;

			dead->state = PS_DEADER;
			process_free(dead);

			return ret;

		default:
			return -1; // this return value isn't used anywhere
			           // TODO enforce that, somehow? idk
	}
}
