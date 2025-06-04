/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"

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
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
    ASSERT(page != NULL);
    ASSERT(type == VM_FILE);

    struct file_lazy_aux *aux = (struct file_lazy_aux *) page->uninit.aux;

    // 파일 seek은 thread-safe하지 않으므로 file_read_at을 사용!
	// TODO: 오작동 시 걍 seek 쓸 것.
    struct file *fl_file = aux->file;
    off_t fl_offset = aux->ofs;
    size_t fl_read_bytes = aux->read_bytes;
    size_t fl_zero_bytes = aux->zero_bytes;
	bool fl_writable = aux->writable;

    // 파일에서 read_bytes만큼 읽기
    if (file_read_at(fl_file, kva, fl_read_bytes, fl_offset) != (int) fl_read_bytes)
        return false;

    // 나머지 영역을 zero-fill
    memset(kva + fl_read_bytes, 0, fl_zero_bytes);

	/* Set up the handler */
	page->operations = &file_ops;

    // file_page 구조로 필요한 정보를 복사
    struct file_page *file_page = &page->file;
    file_page->file = fl_file;
    file_page->ofs = fl_offset;
    file_page->read_bytes = fl_read_bytes;
    file_page->zero_bytes = fl_zero_bytes;
    file_page->writable = fl_writable;

    return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
    // page->file에 실제 정보가 다 채워져 있다면 아래처럼 직접 
	struct file_page *file_page UNUSED = &page->file;
    do_mmap(NULL, file_page->read_bytes, file_page->writable, file_page->file, file_page->ofs);
	return lazy_load_segment(page, file_page);
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	// dirty 시 동기화
	if (pml4_is_dirty(thread_current()->pml4, page->va)) {
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}

	// 상호간 연결 해제
	page->frame->page = NULL;
	page->frame = NULL;

	// pml4에서 clear
	pml4_clear_page(thread_current()->pml4, page->va);

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
    uint64_t *pml4 = thread_current()->pml4;

    // dirty면 write-back (frame이 존재할 때만)
    if (page->frame && pml4_is_dirty(pml4, page->va) && file_page->writable) {
        file_write_at(file_page->file, page->frame->kva, file_page->read_bytes, file_page->ofs);
        pml4_set_dirty(pml4, page->va, 0);
    }

    // 매핑 해제
    pml4_clear_page(pml4, page->va);

    // TODO: 파일 닫기는 필요 시 별도 refcount 관리
    // if (file_page->file) { file_close(file_page->file); file_page->file = NULL; }
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	// 하자있는 요청 내용 차단
    if(length == 0) return NULL;
    if(offset % PGSIZE != 0) return NULL;
    if(file == NULL) return NULL;

    

    void *start_addr = pg_round_down(addr);
    size_t page_cnt = (length + PGSIZE - 1) / PGSIZE;

    if(addr == NULL) addr = palloc_get_multiple(PAL_USER | PAL_ZERO, page_cnt);
    // 파일을 받냐 디스크립터를 받냐가 있는데 일단은 file을 받는다고 전제하자

    void *current_addr = start_addr;
    for (size_t i = 0; i < page_cnt; i++) {
    // aux 구조체 생성
    struct file_lazy_aux *aux = malloc(sizeof(struct file_lazy_aux));
    aux->file = file;
    aux->ofs = offset + (i * PGSIZE);
    aux->read_bytes = length - (i * PGSIZE) < PGSIZE ? length - (i * PGSIZE) : PGSIZE;
    aux->zero_bytes = PGSIZE - aux->read_bytes;
    aux->writable = writable;
    
    // VM_FILE 페이지 생성
    if (!vm_alloc_page_with_initializer(VM_FILE, current_addr, writable,
                                       file_backed_initializer, aux)) {
        // 실패 시 이미 할당된 페이지들 정리
        return NULL;
    }
    current_addr += PGSIZE;
}
}

/* Do the munmap */
void
do_munmap (void *addr) {
    struct supplemental_page_table *spt = &thread_current()->spt;
    
    // 가상 주소로 페이지 찾기
    struct page *page = spt_find_page(spt, addr);
    
    // 페이지를 찾지 못했거나, 페이지가 munmap을 해야 할 것이 아닌 경우
    if (page == NULL || page->operations->type != VM_FILE)
        return false;
    
    return file_write_back(page);
}
