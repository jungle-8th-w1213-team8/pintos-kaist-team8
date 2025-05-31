/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "kernel/hash.h"

#include "threads/vaddr.h"
#include "threads/mmu.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		// 새로운 페이지를 만들어야 하는 경우, 직접 하지말고 vm_alloc_page 쓰세용
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt , void *va) {
	struct page *page = NULL;
	
	// hash_find 함수를 사용해서 spt에 검색 조건 va를 던진다.
	// hash_find는 이러쿵 저러쿵 해서 find_elem을 부른다. find_elem은 못찾으면 NULL 반환.
	// page에 대입하면 된다. NULL이 뜰 수도 있지?
	page = hash_entry(hash_find(&spt->main_table, /*va를 hash_elem으로 생각하기*/), struct page, page_hashelem);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt ,struct page *page) {
	int succ = true;
	// hash_insert 함수를 사용해서 spt에 넣을 물건 page를 던진다.
	// hash_insert는 적절한 위치를 탐색하고, 중복 탐색도 합니다. (중복은 삽입 안됩니다!)
	// insert_elem으로 삽입에 대한 핵심 로직이 이루어집니다.
	struct hash_elem *result = hash_insert(&spt->main_table, &page->page_hashelem);
	if(result == NULL) succ = false;

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	// spt에서 page->va
	
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc()을 호출하고 프레임을 얻습니다. 사용 가능한 페이지가 없으면 페이지를 
 * 축출(evict)하고 반환합니다. 이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀
 * 메모리가 가득 차면, 이 함수는 프레임을 축출하여 사용 가능한 메모리 공간을 확보합니다.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	frame = palloc_get_page(PAL_USER);

	if(frame == NULL)
	{
		// victim page selection and swap out
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	// 이 함수의 행위 책임 :
		// 스택 성장 처리 (ok)
		// 스택 성장 필요 유무를 확인 (?)
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
	// 쓰기 제한이 걸린 페이지를 다뤄야해요
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f , void *addr ,
		bool user , bool write , bool not_present) {
	// page fault면 여기 실행. 즉 주어진 정보를 통해 spt에 있는 내용에서 찾아야한다.

	if(!not_present) return false; // 존재하지도 않는 페이지는 handle 하면 안돼
	if(!user && addr < USER_STACK) return false; // 커널 영역 접근 시도 차단
	

	
	
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, addr);

	if(page == NULL) return false;

	// 
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	// vm에 
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	// mmu.c에는 pml4에 대한 내용을 다루고 있음, pml4에 실제로 올라가는 내용을 다루기
	

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(&thread_current()->pml4, page, frame, 1);

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt ) {
	hash_init(&spt->main_table, hash_bytes, hash_less_standard, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src ) {
	// src에서 dst로 supplemental_page_table 복사하기.
	if(hash_empty(&src->main_table)) return true; // 복사할게 없네용 : true 반환
	
	// 전체순회 박고 죄다 삽입시도 하는 거
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	// spt를 free 하는 내용이 수행된다.
	// 이 함수에 들어오는 spt가 비어있는지에 대한 보장이 없다. 그래서 전체 순회 박으면서 free부터 한다.
	// 그리고 마지막에 kill the hash
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}


bool hash_less_standard(struct hash_elem *A, struct hash_elem *B)
{
	struct page *pageA = hash_entry(A, struct page, page_hashelem);
	struct page *pageB = hash_entry(B, struct page, page_hashelem);
	return pageA->va > pageB->va;
}
