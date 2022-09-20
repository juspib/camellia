#pragma once
#include <kernel/arch/generic.h>
#include <kernel/handle.h>
#include <stdbool.h>
struct vfs_mount;

#define HANDLE_MAX 16

enum process_state {
	PS_RUNNING,
	PS_DEAD, // return message not collected
	PS_WAITS4CHILDDEATH,
	PS_WAITS4FS,
	PS_WAITS4REQUEST,
	PS_WAITS4PIPE,
	PS_WAITS4TIMER,

	PS_LAST,
};

struct process {
	struct pagedir *pages;
	/* if NULL, refcount == 1. kmalloc'd */
	uint64_t *pages_refcount;

	struct registers regs;
	struct process *sibling, *child, *parent;

	enum process_state state;
	union { /* saved value, meaning depends on .state */
		int death_msg; // PS_DEAD
		struct {
			char __user *buf;
			size_t max_len;
			struct ufs_request __user *res;
		} awaited_req; // PS_WAITS4REQUEST
		struct {
			struct handle *pipe;
			char __user *buf;
			size_t len;
			struct process *next;
		} waits4pipe;
		struct {
			/* managed by timer_schedule */
			uint64_t goal;
			struct process *next;
		} waits4timer;
	};

	struct vfs_mount *mount;
	struct handle **_handles; /* points to struct handle *[HANDLE_MAX] */
	uint64_t *handles_refcount; /* works just like pages_refcount */

	uint32_t cid; /* child id. unique amongst all of this process' siblings */
	uint32_t nextcid; /* the child id to assign to the next spawned child */
	uint32_t globalid; /* only for debugging, don't expose to userland */
	bool noreap;

	/* allocated once, the requests from WAITS4FS get stored here */
	struct vfs_request *reqslot;

	/* vfs_backend controlled (not exclusively) by this process */
	struct vfs_backend *controlled;

	struct {
		void *buf;
		size_t len;
		size_t pos;
	} execbuf;
};

extern struct process *process_current;

/** Creates the root process. */
struct process *process_seed(void *data, size_t datalen);
struct process *process_fork(struct process *parent, int flags);

void process_kill(struct process *proc, int ret);
/** Tries to free a process / collect its return value. */
void process_try2collect(struct process *dead);

/** Switches execution to any running process. */
_Noreturn void process_switch_any(void);

/** Used for iterating over all processes */
struct process *process_next(struct process *);

handle_t process_find_free_handle(struct process *proc, handle_t start_at);
struct handle *process_handle_get(struct process *, handle_t);
handle_t process_handle_init(struct process *, enum handle_type, struct handle **);
handle_t process_handle_dup(struct process *p, handle_t from, handle_t to);
static inline void process_handle_close(struct process *p, handle_t hid) {
	// TODO test
	process_handle_dup(p, -1, hid);
}

/* Gets a handle and removes the process' reference to it, without decreasing the refcount.
 * Meant to be used together with process_handle_put. */
struct handle *process_handle_take(struct process *, handle_t);
/* Put a handle in a process, taking the ownership away from the caller.
 * Doesn't increase the refcount on success, decreases it on failure. */
handle_t process_handle_put(struct process *, struct handle *);

void process_transition(struct process *, enum process_state);
