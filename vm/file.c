/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"
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
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
    ASSERT(page != NULL);
    ASSERT(type == VM_FILE);
    struct file_lazy_aux *aux = (struct file_lazy_aux *) page->uninit.aux;
	page->operations = &file_ops;
    return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {

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
	// dirty 시 동기화
	if (pml4_is_dirty(thread_current()->pml4, page->va)) {
	 	file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
	 	pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	// 상호간 연결 해제
    //page->frame->page = NULL;
	//page->frame = NULL;

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
		addr += PGSIZE;
	}
}

// /* Do the mmap */
// void *
// do_mmap (void *addr, size_t length, int writable,
// 		struct file *file, off_t offset) {
// 	// 하자있는 요청 내용 차단
//     if(pg_round_down(addr) != addr) return NULL;
//     if(length == 0) return NULL;
//     if(offset % PGSIZE != 0) return NULL;
//     if(file == NULL) return NULL;
//     if(is_kernel_vaddr(addr)) return NULL;
//     if(is_kernel_vaddr(length)) return NULL;
	
// 	//lock_acquire(&filesys_lock);
//     struct file *r_file = file_reopen(file);
//     //   if(offset + length > file_length(r_file)) return NULL;
// 	//lock_acquire(&filesys_lock);
//     if(addr == NULL) // called by NOT user
//     {
//         return NULL;
//     }
//     else // called by USER
//     {

//         void *start_addr = pg_round_down(addr);
//         size_t page_cnt = (length + PGSIZE - 1) / PGSIZE;
//         int *ref_count = malloc(sizeof(int));
//         *ref_count = page_cnt;
    
//         void *current_addr = start_addr;
//         for (size_t i = 0; i < page_cnt; i++)
//         {
//             // aux 구조체 생성
//             struct file_lazy_aux *aux = malloc(sizeof(struct file_lazy_aux));
//             aux->file = r_file;
//             aux->ofs = offset + (i * PGSIZE);
//             size_t remaining = length > i * PGSIZE ? length - i * PGSIZE : 0;
//             aux->read_bytes = remaining < PGSIZE ? remaining : PGSIZE;
//            //aux->read_bytes = length - (i * PGSIZE) < PGSIZE ? length - (i * PGSIZE) : PGSIZE;
//             aux->zero_bytes = PGSIZE - aux->read_bytes;
//             aux->writable = writable;
//             aux->ref_count = ref_count;
//         // VM_FILE 페이지 생성
//             if (!vm_alloc_page_with_initializer(VM_FILE, current_addr, writable, lazy_load_segment, aux)) {
//                     current_addr = start_addr;
//                     for (size_t j = 0; j < i; j++)
//                     {
//                         struct page *pg = pml4_get_page(thread_current()->pml4, current_addr);
//                         spt_remove_page(&thread_current()->spt, pg);
//                         vm_dealloc_page(pg);
//                         current_addr += PGSIZE;
//                     }     
//                 file_close(r_file);
//                 return NULL;
//             }
//             else
//             {
//                 // only for debug
//             }
//             current_addr += PGSIZE;
//         }
//     }
//     return addr;
// }


// void do_munmap (void *addr) {
//     struct supplemental_page_table *spt = &thread_current()->spt;
//     struct page *first_page = spt_find_page(spt, addr);
    
//     if (!first_page) return;
    
//     // uninit 페이지인 경우 파일 정보를 aux에서 가져 올 것이다
//     struct file *target_file;
//     if (first_page->operations->type == VM_UNINIT) {
//         struct file_lazy_aux *aux = first_page->uninit.aux;
//         target_file = aux->file;
//     } else {
//         target_file = first_page->file.file;
//     }
    
//     void *cur_addr = addr;
//     while (true) {
//         struct page *page = spt_find_page(spt, cur_addr);
//         if (!page) break;
        
//         // 파일 정보 안전하게 확인
//         struct file *page_file;
//         if (page->operations->type == VM_UNINIT) {
//             struct file_lazy_aux *aux = page->uninit.aux;
//             page_file = aux->file;
//         } else if (page->operations->type == VM_FILE) {
//             page_file = page->file.file;
//         } else {
//             break;  // 다른 타입이면 중단
//         }
        
//         if (page_file != target_file) break;
        
//         spt_remove_page(spt, page);
// 		//pml4_clear_page(thread_current()->pml4, page->va);
//         vm_dealloc_page(page);
//         cur_addr += PGSIZE;
//     }
// }

