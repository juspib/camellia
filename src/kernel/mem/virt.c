#include <kernel/arch/generic.h>
#include <kernel/mem/virt.h>
#include <kernel/util.h>
#include <shared/mem.h>

void virt_iter_new(
		struct virt_iter *iter, void __user *virt, size_t length,
		struct pagedir *pages, bool user, bool writeable)
{
	iter->frag       = NULL;
	iter->frag_len   = 0;
	iter->prior      = 0;
	iter->error      = false;
	iter->_virt      = virt;
	iter->_remaining = length;
	iter->_pages     = pages;
	iter->_user      = user;
	iter->_writeable = writeable;
}

bool virt_iter_next(struct virt_iter *iter) {
	/* note: While i'm pretty sure that this should always work, this
	 * was only tested in cases where the pages were consecutive both in
	 * virtual and physical memory, which might not always be the case.
	 * TODO test this */

	size_t partial = iter->_remaining;
	iter->prior   += iter->frag_len;
	if (partial <= 0) return false;

	if (iter->_pages) { // if iterating over virtual memory
		// don't cross page boundaries
		size_t to_page = PAGE_SIZE - ((uintptr_t)iter->_virt & PAGE_MASK);
		partial = min(partial, to_page);

		iter->frag = pagedir_virt2phys(iter->_pages,
				iter->_virt, iter->_user, iter->_writeable);

		if (!iter->frag) {
			iter->error = true;
			return false;
		}
	} else {
		// "iterate" over physical memory
		// the double cast supresses the warning about changing address spaces
		iter->frag = (void* __force)iter->_virt;
	}

	iter->frag_len    = partial;
	iter->_remaining -= partial;
	iter->_virt      += partial;
	return true;
}

bool virt_cpy(
		struct pagedir *dest_pages,       void __user *dest,
		struct pagedir  *src_pages, const void __user *src, size_t length)
{
	struct virt_iter dest_iter, src_iter;
	size_t cur_len;

	virt_iter_new(&dest_iter,           dest, length, dest_pages, true, true);
	virt_iter_new( &src_iter, (userptr_t)src, length,  src_pages, true, false);
	dest_iter.frag_len = 0;
	src_iter.frag_len  = 0;

	for (;;) {
		if (dest_iter.frag_len <= 0)
			if (!virt_iter_next(&dest_iter)) break;
		if ( src_iter.frag_len <= 0)
			if (!virt_iter_next( &src_iter)) break;

		cur_len = min(src_iter.frag_len, dest_iter.frag_len);
		memcpy(dest_iter.frag, src_iter.frag, cur_len);

		dest_iter.frag_len -= cur_len;
		dest_iter.frag     += cur_len;
		src_iter.frag_len  -= cur_len;
		src_iter.frag      += cur_len;
	}

	return !(dest_iter.error || src_iter.error);
}
