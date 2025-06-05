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

    size_t file_len = file_length(fl_file);

    // 실제로 읽을 수 있는 양 계산
    size_t available = 0;
    if (fl_offset < file_len) {
        available = file_len - fl_offset;
        if (available > fl_read_bytes)
            available = fl_read_bytes;
    }
    // 파일에서 read_bytes만큼 읽기
    off_t actually = file_read_at(fl_file, kva, fl_read_bytes, fl_offset);
    if (actually != available)
    {
        return false;
    }
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
	struct file_page *file_page = &page->file;

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
	//pml4_clear_page(page->owner->pml4, page->va);
	pml4_clear_page(thread_current()->pml4, page->va);
    
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

    // TODO: 파일 닫기는 필요 시 별도 refcount 관리
    // lazy-file 통과 여부, 여기서 주석 처리하면 통과함
     (file_page->ref_count)--;
     // 해당 파일을 참조 하고 있는 ref_count를 세어봅니다. 0이 되는경우 해당 파일의 원본이라고 전제하고 그제야 file_close가 진행됩니다.
if (file_page->ref_count == 0) {
    file_close(file_page->file);
    free(file_page->ref_count);
    file_page->file = NULL;
}
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	// 하자있는 요청 내용 차단
    if(pg_round_down(addr) != addr) return NULL;
    if(length == 0) return NULL;
    if(offset % PGSIZE != 0) return NULL;
    if(file == NULL) return NULL;
    if(is_kernel_vaddr(addr)) return NULL;
    if(is_kernel_vaddr(length)) return NULL;

    struct file *r_file = file_reopen(file);
    //   if(offset + length > file_length(r_file)) return NULL;

    if(addr == NULL) // called by NOT user
    {
        return NULL;
    }
    else // called by USER
    {

        void *start_addr = pg_round_down(addr);
        size_t page_cnt = (length + PGSIZE - 1) / PGSIZE;
        int *ref_count = malloc(sizeof(int));
        *ref_count = page_cnt;
    
        void *current_addr = start_addr;
        for (size_t i = 0; i < page_cnt; i++)
        {
            // aux 구조체 생성
            struct file_lazy_aux *aux = malloc(sizeof(struct file_lazy_aux));
            aux->file = r_file;
            aux->ofs = offset + (i * PGSIZE);
            aux->read_bytes = length - (i * PGSIZE) < PGSIZE ? length - (i * PGSIZE) : PGSIZE;
            aux->zero_bytes = PGSIZE - aux->read_bytes;
            aux->writable = writable;
            aux->ref_count = ref_count;
        // VM_FILE 페이지 생성
            if (!vm_alloc_page_with_initializer(VM_FILE, current_addr, writable, lazy_load_segment, aux)) {
                    current_addr = start_addr;
                    for (size_t j = 0; j < i; j++)
                    {
                        vm_dealloc_page(pml4_get_page(thread_current()->pml4, current_addr));
                        current_addr += PGSIZE;
                    }     
                file_close(r_file);
                return NULL;
            }
            else
            {
            }
            current_addr += PGSIZE;
        }
    }
    return addr;
}



void do_munmap (void *addr) {
    struct supplemental_page_table *spt = &thread_current()->spt;
    struct page *first_page = spt_find_page(spt, addr);
    
    if (!first_page) return;
    
    // uninit 페이지인 경우 파일 정보를 aux에서 가져 올 것이다
    struct file *target_file;
    if (first_page->operations->type == VM_UNINIT) {
        struct file_lazy_aux *aux = first_page->uninit.aux;
        target_file = aux->file;
    } else {
        target_file = first_page->file.file;
    }
    
    void *cur_addr = addr;
    while (true) {
        struct page *page = spt_find_page(spt, cur_addr);
        if (!page) break;
        
        // 파일 정보 안전하게 확인
        struct file *page_file;
        if (page->operations->type == VM_UNINIT) {
            struct file_lazy_aux *aux = page->uninit.aux;
            page_file = aux->file;
        } else if (page->operations->type == VM_FILE) {
            page_file = page->file.file;
        } else {
            break;  // 다른 타입이면 중단
        }
        
        if (page_file != target_file) break;
        
        spt_remove_page(spt, page);
        vm_dealloc_page(page);
        cur_addr += PGSIZE;
    }
}
