#include <kernel/arch/generic.h>
#include <kernel/main.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/virt.h>
#include <kernel/panic.h>
#include <kernel/proc.h>
#include <kernel/vfs/mount.h>
#include <shared/mem.h>
#include <stdint.h>

struct process *process_first;
struct process *process_current;
static struct process *process_deadparent;

static uint32_t next_pid = 0;

struct process *process_seed(struct kmain_info *info) {
	process_first = kmalloc(sizeof *process_first);
	memset(process_first, 0, sizeof *process_first);
	process_first->state = PS_RUNNING;
	process_first->pages = pagedir_new();
	process_first->mount = vfs_mount_seed();
	process_first->id    = next_pid++;

	// map the stack to the last page in memory
	pagedir_map(process_first->pages, (userptr_t)~PAGE_MASK, page_alloc(1), true, true);
	process_first->regs.esp = (userptr_t) ~0xF;

	// map the kernel
	//   yup, .text is writeable too. the plan is to not map the kernel
	//   into user memory at all, but i'll implement that later. TODO
	for (size_t p = 0x100000; p < (size_t)&_bss_end; p += PAGE_SIZE)
		pagedir_map(process_first->pages, (userptr_t)p, (void*)p, false, true);

	// map the init module as rw
	void __user *init_base = (userptr_t)0x200000;
	for (uintptr_t off = 0; off < info->init.size; off += PAGE_SIZE)
		pagedir_map(process_first->pages, init_base + off, info->init.at + off,
		            true, true);
	process_first->regs.eip = init_base;

	process_deadparent = kmalloc(sizeof *process_deadparent);
	memset(process_deadparent, 0, sizeof *process_deadparent);
	process_deadparent->state = PS_DUMMY;
	process_deadparent->id    = next_pid++;

	process_first->sibling = process_deadparent;

	return process_first;
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

	if (child->controlled)
		child->controlled->potential_handlers++;

	for (handle_t h = 0; h < HANDLE_MAX; h++) {
		if (child->handles[h])
			child->handles[h]->refcount++;
		// no overflow check - if you manage to get 2^32 references to a handle you have bigger problems
	}

	child->id = next_pid++;

	return child;
}

void process_forget(struct process *p) {
	assert(p->parent);

	if (p->parent->child == p) {
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
}

void process_free(struct process *p) {
	bool valid = false;
	if (p->state == PS_DEADER) valid = true;
	if (p->state == PS_DEAD && (!p->parent
	                        ||   p->parent->state == PS_DEAD
	                        ||   p->parent->state == PS_DEADER)) valid = true;
	assert(valid);

	while (p->child)
		process_free(p->child);

	if (!p->parent) return;
	process_forget(p);
	pagedir_free(p->pages); // TODO could be done on kill
	kfree(p);
}

static _Noreturn void process_switch(struct process *proc) {
	assert(proc->state == PS_RUNNING);
	if (proc->deathbed) {
		process_kill(proc, -1);
		process_switch_any();
	}
	process_current = proc;
	pagedir_switch(proc->pages);
	sysexit(proc->regs);
}

/** If there are any processes waiting for IRQs, wait with them. Otherwise, shut down */
static _Noreturn void process_idle(void) {
	struct process *procs[16];
	size_t len = process_find_multiple(PS_WAITS4IRQ, procs, 16);

	if (len == 0) shutdown();

	if (process_first->state == PS_DEAD || process_first->state == PS_DEADER) {
		/* special case: if the system is about to shut off, stop waiting for IRQs
		 * usually this would be an issue because it would let processes know if
		 * they're using the kernel handler, but the system is shutting off anyways,
		 * and no further user code will run */
		for (size_t i = 0; i < len; i++) {
			assert(procs[i]->deathbed);
			process_transition(procs[i], PS_RUNNING);
		}
		process_switch_any(); // start cleaning out processes
	}

	for (;;) {
		for (size_t i = 0; i < len; i++) {
			if (procs[i]->waits4irq.ready()) {
				/* if this is entered during the first iteration, it indicates a
				 * kernel bug. this should be logged. TODO? */
				procs[i]->waits4irq.callback(procs[i]);
				process_switch_any();
			}
		}
		cpu_pause();
	}
}

_Noreturn void process_switch_any(void) {
	if (process_current && process_current->state == PS_RUNNING)
		process_switch(process_current);

	struct process *found = process_find(PS_RUNNING);
	if (found) process_switch(found);
	process_idle();
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

handle_t process_find_handle(struct process *proc, handle_t start_at) {
	// TODO start_at is a bit of a hack
	handle_t handle;
	for (handle = start_at; handle < HANDLE_MAX; handle++) {
		if (proc->handles[handle] == NULL)
			break;
	}
	if (handle >= HANDLE_MAX) handle = -1;
	return handle;
}

struct handle*
process_handle_get(struct process *p, handle_t id, enum handle_type type) {
	struct handle *h;
	if (id < 0 || id >= HANDLE_MAX) return NULL;
	h = p->handles[id];
	if (h == NULL || h->type != type) return NULL;
	return h;
}

void process_transition(struct process *p, enum process_state state) {
	enum process_state last = p->state;
	p->state = state;
	switch (state) {
		case PS_RUNNING:
			assert(last != PS_DEAD && last != PS_DEADER);
			break;
		case PS_DEAD:
			// see process_kill
			break;
		case PS_DEADER:
			assert(last == PS_DEAD);
			process_free(p);
			break;
		case PS_WAITS4CHILDDEATH:
		case PS_WAITS4FS:
		case PS_WAITS4REQUEST:
			assert(last == PS_RUNNING);
			break;
		case PS_WAITS4IRQ:
			assert(last == PS_WAITS4FS);
			break;

		case PS_DUMMY:
		case PS_LAST:
			panic_invalid_state();
	}
}

void process_kill(struct process *p, int ret) {
	if (p->state == PS_DEAD || p->state == PS_DEADER) return;

	if (p->handled_req) {
		vfs_request_cancel(p->handled_req, ret);
		p->handled_req = NULL;
	}
	if (p->controlled) {
		// code stink: i don't like how handling controlled backends is split
		// between this if and the switch lower down
		p->controlled->potential_handlers--;
		if (p->controlled->potential_handlers == 0) {
			// orphaned
			struct vfs_request *q = p->controlled->queue;
			while (q) {
				struct vfs_request *q2 = q->queue_next;
				vfs_request_cancel(q, ret);
				q = q2;
			}
		}
	}

	// TODO VULN unbounded recursion
	struct process *c2;
	for (struct process *c = p->child; c; c = c2) {
		c2 = c->sibling;
		process_kill(c, -1);
	}

	switch (p->state) {
		case PS_RUNNING:
		case PS_WAITS4CHILDDEATH:
			break;

		case PS_WAITS4FS:
			// if the request wasn't accepted we could just remove this process from the queue
		case PS_WAITS4IRQ:
			/* instead of killing the process outright, we mark it to get killed
			 * as soon as it becomes running and we try to give it control.
			 *
			 * we also reparent it to process_deadparent because we don't want
			 * dead processes to have any alive children */
			/* TODO: because requests are no longer owned by the parent, we can safely kill it.
			 * this whole deathbed thing (and, by extension, freeing after killing) is unnecessary */
			// TODO process_reparent?
			p->deathbed = true;
			process_forget(p);
			p->sibling = process_deadparent->child;
			p->parent  = process_deadparent;
			process_deadparent->child  = p;
			return;

		case PS_WAITS4REQUEST:
			assert(p->controlled);
			if (p->controlled->handler == p)
				p->controlled->handler = NULL;
			break;

		case PS_DEAD:
		case PS_DEADER:
		case PS_DUMMY:
		case PS_LAST:
			kprintf("process_kill unexpected state 0x%x\n", p->state);
			panic_invalid_state();
	}

	for (handle_t h = 0; h < HANDLE_MAX; h++)
		handle_close(p->handles[h]);
	process_transition(p, PS_DEAD);
	p->death_msg = ret;
	process_try2collect(p);
}

int process_try2collect(struct process *dead) {
	struct process *parent = dead->parent;
	int ret;

	assert(dead->state == PS_DEAD);

	if (!parent) {
		process_transition(dead, PS_DEADER);
		return -1;
	}
	switch (parent->state) {
		case PS_WAITS4CHILDDEATH:
			ret = dead->death_msg;
			regs_savereturn(&parent->regs, ret);
			process_transition(parent, PS_RUNNING);
			process_transition(dead, PS_DEADER);

			return ret;

		case PS_DEAD:
		case PS_DEADER:
		case PS_DUMMY:
			process_transition(dead, PS_DEADER);
			return -1;

		default:
			return -1; // this return value isn't used anywhere
			           // TODO enforce that, somehow? idk
	}
}
