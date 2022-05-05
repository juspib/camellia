#include <kernel/arch/i386/driver/ps2.h>
#include <kernel/arch/i386/interrupts/irq.h>
#include <kernel/mem/virt.h>
#include <kernel/panic.h>
#include <shared/container/ring.h>
#include <shared/mem.h>


#define BACKLOG_CAPACITY 64
static volatile uint8_t backlog_buf[BACKLOG_CAPACITY];
static volatile ring_t backlog = {(void*)backlog_buf, BACKLOG_CAPACITY, 0, 0};

static struct vfs_request *blocked_on = NULL;

bool ps2_ready(void) {
	return ring_size((void*)&backlog) > 0;
}

void ps2_recv(uint8_t s) {
	ring_put1b((void*)&backlog, s);
	if (blocked_on) {
		vfs_ps2_accept(blocked_on);
		blocked_on = NULL;
	}
}

size_t ps2_read(uint8_t *buf, size_t len) {
	return ring_get((void*)&backlog, buf, len);
}

void vfs_ps2_accept(struct vfs_request *req) {
	// when you fix something here go also fix it in the COM1 driver
	static uint8_t buf[32]; // pretty damn stupid
	int ret;
	switch (req->type) {
		case VFSOP_OPEN:
			// allows opening /ps2/anything, TODO don't
			vfsreq_finish(req, 0); // fake file handle, whatever
			break;
		case VFSOP_READ:
			if (ps2_ready()) {
				// TODO FUKKEN MESS
				if (req->caller) {
					// clamp between 0, sizeof buf
					ret = req->output.len;
					if (ret > sizeof buf) ret = sizeof buf;
					if (ret < 0) ret = 0;

					ret = ps2_read(buf, ret);
					virt_cpy_to(req->caller->pages, req->output.buf, buf, ret);
				} else ret = -1;
				vfsreq_finish(req, ret);
			} else {
				blocked_on = req;
			}
			break;
		default:
			vfsreq_finish(req, -1);
			break;
	}
}

bool vfs_ps2_ready(struct vfs_backend *self) { return blocked_on == NULL; }
