/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

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
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	// 메인 메모리로 파일 내용을 가져오기
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	// 디스크로 파일을 빼기. 대신, 쓴 기록은 전부 반영시켜야함
}

/* Destroy the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	// 쓰기 여부를 확인하자. 쓰이지 않았으면 할 것도 없음
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	// TODO : 적절하지 않은 내용에 대해 차단 후 본 루틴을 실행 할 것 !!
	
	// #1 주어진 매개변수의 유효성 검사


	// #2 #1 통과시 alloc page 호출을 위한 적절 변환 진행


	// #3 vm_alloc_page_with_initializer()
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
