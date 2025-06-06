/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
    ASSERT(page != NULL);
    ASSERT(type == VM_FILE);

	page->operations = &file_ops;
	struct file_page *file_page UNUSED = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool file_backed_swap_in (struct page *page, void *kva) {
    // page->file에 실제 정보가 다 채워져 있다면 아래처럼 직접 
	struct file_page *file_page UNUSED = &page->file;
	struct file_lazy_aux *aux = (struct file_lazy_aux *) page->uninit.aux;
	struct file *file = aux->file;
	off_t offset = aux->ofs;
	size_t page_read_bytes = aux->read_bytes;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;


    lock_acquire(&g_filesys_lock);  
	// reading the contents in from the file = load_segment
    // file_read_at을 사용!
    if (file_read_at(aux->file, kva, aux->read_bytes, aux->ofs) != (int) aux->read_bytes) {
		// palloc_free_page(kva);
		// free(aux);
    	lock_release(&g_filesys_lock);  
        return false;
    }
    lock_release(&g_filesys_lock);

	memset(kva + page_read_bytes, 0, page_zero_bytes);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *curr = thread_current();
	struct file_lazy_aux *aux;

	// first check if the page is dirty
	if (pml4_is_dirty(curr->pml4, page->va)){
		aux = (struct file_lazy_aux *) page->uninit.aux;

		// writing the contents back to the file.
		file_write_at(aux->file, page->frame->kva, aux->read_bytes, aux->ofs);

		// After you swap out the page, remember to turn off the dirty bit for the page.
		pml4_set_dirty (curr->pml4, page->va, 0);
	}

	pml4_clear_page(curr->pml4, page->va);	// 페이지 테이블에서는 지워주기
	return true;
}


/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
    uint64_t *pml4 = thread_current()->pml4;
    // dirty면 write-back (frame이 존재할 때만)
    if (page->frame && pml4_is_dirty(pml4, page->va)) {
        file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
        pml4_set_dirty(pml4, page->va, 0);
    }

    // 매핑 해제
    pml4_clear_page(pml4, page->va);
}

/* Do the mmap */
void* do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	// virtual pages starting at addr
	void * mapped_va = addr;

	// Set these bytes to zero
    size_t read_bytes = length > (size_t)file_length(file) ? (size_t)file_length(file) : length;
    size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;
	
	// obtain a separate and independent reference

	lock_acquire(&g_filesys_lock);
	struct file *mapping_file = file_reopen(file);
	lock_release(&g_filesys_lock);  

	// starting from offset byte
	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_lazy_aux *aux;
		aux = (struct file_lazy_aux *)malloc(sizeof(struct file_lazy_aux));

		aux->file = mapping_file;
		aux->ofs = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;

		// return NULL which is not a valid address to map a file
		if (!vm_alloc_page_with_initializer (VM_FILE, addr,
					writable, lazy_load_segment, aux)){
			free(aux);
			return NULL;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}
	// returns the virtual address where the file is mapped
	return mapped_va;
}

/* Do the munmap */
void do_munmap (void *addr) {
	// the specified address range addr
	struct thread *curr = thread_current();
	struct page *page;
	struct file_lazy_aux* aux;

	// 파일이 끝날 때까지 반복
	while (true){
		// 파일 찾기
		page = spt_find_page(&curr->spt, addr);
        // 파일의 끝인지 확인
		if (!page || page_get_type(page) != VM_FILE)
            break;
		
		// written back to the file
		if (pml4_is_dirty(curr->pml4, page->va)){
			aux = (struct file_lazy_aux *) page->uninit.aux;

			lock_acquire(&g_filesys_lock);
			file_write_at(aux->file, addr, aux->read_bytes, aux->ofs);
			lock_release(&g_filesys_lock);

            pml4_set_dirty (curr->pml4, page->va, 0);
		}

		// unmap
		pml4_clear_page(curr->pml4, page->va);
		spt_remove_page(&curr->spt, page);
		addr += PGSIZE;
	}
}
