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
// 특별히 할 건 없다
void
vm_file_init (void) {}

/* Initialize the file backed page */
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
    ASSERT(page != NULL);
    ASSERT(type == VM_FILE);

	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool file_backed_swap_in (struct page *page, void *kva) {
    // page->file에 실제 정보가 다 채워져 있다면 아래처럼 직접 
	struct file_page *file_page = &page->file;
	struct file_lazy_aux *aux = (struct file_lazy_aux *) page->uninit.aux;
	struct file *file = aux->file;
	off_t offset = aux->ofs;
	size_t page_read_bytes = aux->read_bytes;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

    lock_acquire(&g_filesys_lock);  
    // file_read_at을 사용
    if (file_read_at(aux->file, kva, aux->read_bytes, aux->ofs) != (int) aux->read_bytes) {
    	lock_release(&g_filesys_lock);  
        return false;
    }
    lock_release(&g_filesys_lock);

	memset(kva + page_read_bytes, 0, page_zero_bytes);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *curr = thread_current();
	struct file_lazy_aux *aux;

	// 페이지가 dirty? 해당 파일에 write back.
	if (pml4_is_dirty(curr->pml4, page->va)){
		aux = (struct file_lazy_aux *) page->uninit.aux;

    	lock_acquire(&g_filesys_lock);  
		file_write_at(aux->file, page->frame->kva, aux->read_bytes, aux->ofs);
    	lock_release(&g_filesys_lock);  

		// Write back 후 dirty bit를 원상 복구한다
		pml4_set_dirty (curr->pml4, page->va, 0);
	}
	
	// 해당 페이지를 페이지 테이블에서 clear.
	pml4_clear_page(curr->pml4, page->va);	
	return true;
}

/* Destroy the file-backed page. PAGE will be freed by the caller. */
static void file_backed_destroy(struct page *page) {
	ASSERT(page != NULL);

	struct thread *curr = thread_current();
	struct file_lazy_aux *aux = (struct file_lazy_aux *) page->uninit.aux;

	// 페이지가 dirty ==> 해당 파일에 write back.
	if (pml4_is_dirty(curr->pml4, page->va) && page->writable) {
		lock_acquire(&g_filesys_lock);
		file_write_at(aux->file, page->frame->kva, aux->read_bytes, aux->ofs);
		lock_release(&g_filesys_lock);

		// Write back 후 dirty bit를 원상 복구.
		pml4_set_dirty(curr->pml4, page->va, false);
	}

	// 유저 페이지 매핑을 클리어
	pml4_clear_page(curr->pml4, page->va);

	// 프레임이 존재할 경우 free.
	if (page->frame != NULL) {
		struct frame *f = page->frame;

		// 프레임 테이블에서 해당 프레임을 삭제
		lock_acquire(&g_frame_lock);
		list_remove(&f->f_elem);
		lock_release(&g_frame_lock);

		// 물리 프레임 및 페이지 free
		palloc_free_page(f->kva);
		free(f);

		page->frame = NULL;
	}

	// aux 존재할 경우 free
	if (aux != NULL) {
		free(aux);
		page->uninit.aux = NULL;
	}
}

/* Do the mmap */
void* do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset) {
	// 리턴을 위한 주소
	void *mapped_addr = addr;

	// 파일에서 read할 분량 / zero 채울 분량 계산
	size_t file_length_bytes = (size_t) file_length(file);
	size_t read_bytes = length < file_length_bytes ? length : file_length_bytes;
	size_t zero_bytes = (PGSIZE - (read_bytes % PGSIZE)) % PGSIZE;
	size_t total_pages = (read_bytes + zero_bytes) / PGSIZE;

	// 이래야 별도의 file descriptor가 되기 때문.
	lock_acquire(&g_filesys_lock);
	struct file *mapping_file = file_reopen(file);
	lock_release(&g_filesys_lock);

	// 각 페이지를 매칭
	for (size_t i = 0; i < total_pages; i++) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		// lazy-load를 위한 aux 설정
		struct file_lazy_aux *aux = malloc(sizeof(struct file_lazy_aux));
		if (aux == NULL) 
			return NULL;
		
		aux->file = mapping_file;
		aux->ofs = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;

		// lazy-load 페이지
		if (!vm_alloc_page_with_initializer(
					VM_FILE, addr, writable, lazy_load_segment, aux)) {
			free(aux);
			return NULL;
		}

		// 다음 페이지로 가자
		read_bytes -= page_read_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}

	// mmap 시작 주소를 리턴
	return mapped_addr;
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
        // 파일의 끝인지 확인 - 안되면 그냥 break!
		if (!page || page_get_type(page) != VM_FILE)
            break;
		
		// 파일 더티면 write back... 알지?
		if (pml4_is_dirty(curr->pml4, page->va)){
			aux = (struct file_lazy_aux *) page->uninit.aux;

			lock_acquire(&g_filesys_lock);
			file_write_at(aux->file, addr, aux->read_bytes, aux->ofs);
			lock_release(&g_filesys_lock);

            pml4_set_dirty (curr->pml4, page->va, 0);
		}

		// unmap
		pml4_clear_page(curr->pml4, page->va);
		addr += PGSIZE;
	}
}
