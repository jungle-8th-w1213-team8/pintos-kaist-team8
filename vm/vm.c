/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "kernel/hash.h"

#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "vm/anon.h"
#include "vm/file.h"

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

	// 주의! upage는 page가 아니다.
	ASSERT (VM_TYPE(type) != VM_UNINIT);
	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if(spt_find_page(spt, upage) != NULL) goto err;
	
	// initalizer를 따로 빼놓고 case마다 배정해준다음 uninit에 배정때리는 방법이 없나..
	struct page *page = malloc(sizeof (struct page));
	// set the operations of page
	switch(VM_TYPE(type))
	{
		case VM_ANON:
			uninit_new(page, upage ,init, type, aux, anon_initializer);
			break;
		case VM_FILE:
			uninit_new(page, upage ,init, type, aux, file_backed_initializer);
			break;
	}
	page->writable = writable;
	bool result = spt_insert_page(spt, page);
	if(!result) goto err;
	
	return true;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt , void *va) {
	// va를 검색조건으로 하는 특정한 page를 탐색 시도합니다.
	// va를 find 함수에 쓰도록 하기 위해서 dummy page를 정의합니다.
	struct page dummy;
	dummy.va = pg_round_down(va);

	lock_acquire(&spt->spt_lock);
	struct hash_elem *he = hash_find(&spt->main_table, &dummy.page_hashelem);
	lock_release(&spt->spt_lock);
	if(he == NULL) return NULL;

	struct page *page = hash_entry(he, struct page, page_hashelem);
	// 과연 hash_entry가 실패 할 수 잇을 가요?!!#@
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt ,struct page *page) {
	// SPT에 페이지를 삽입합니다. hash_insert는 중복 삽입을 허용하지 않아 false의 여지가 있습니다.
	bool succ = true;
	struct hash_elem *result = hash_insert(&spt->main_table, &page->page_hashelem);
	if(result != NULL) succ = false;
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	// SPT에서 페이지를 제거합니다. 이어서 해당 페이지의 메모리를 FREE 합니다.
	hash_delete(&spt->main_table, &page->page_hashelem);
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
	frame = malloc(sizeof (struct frame));

	frame->kva = palloc_get_page(PAL_USER);
	if(frame->kva == NULL)
	{
		free(frame);
		frame = vm_evict_frame(); // victim page selection and swap out
		if(frame == NULL) return NULL;
	}

	frame->page = NULL;

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

	// 우선, 적절한 Fault 호출인지부터 봐야 한다.
	if(!not_present) return false; // 존재하지도 않는 페이지는 handle 하면 안돼
	if(!user && addr < USER_STACK) return false; // 커널 영역 접근 시도 차단
	// if (!write) return false;
	// 이건 지금 필요한지 잘 모르겠어요
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, addr);
	if(page == NULL) return false;
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
vm_claim_page (void *va) {
	if(va > USER_STACK) return false;

	struct page *page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL) return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
// 받은 페이지를 Frame에 담고 PML4에 배치해야해요.
// Frame을 배정 받지 못할 수 있고, PML4 배치에도 실패 할 수 있어요.
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	bool succ = true;
	if(frame == NULL) return false;
	
	frame->page = page;
	page->frame = frame;

	succ = pml4_set_page(&thread_current()->pml4, pg_round_down(page->va), pg_round_down(frame->kva), 1);
	if(!succ) return false;
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt ) {
	hash_init(&spt->main_table, hash_bytes, hash_less_standard, NULL);
	lock_init(&spt->spt_lock);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src ) {
	if(hash_empty(&src->main_table)) return true;
	
	lock_acquire(&src->spt_lock);
	struct hash_iterator i;
	hash_first (&i, src);
	while (hash_next (&i))
	{
		struct page *targetCopy = hash_entry(hash_cur (&i), struct page, page_hashelem);
		hash_insert(&dst->main_table, &targetCopy->page_hashelem);
	}
	lock_release(&src->spt_lock);
	// dst는 fork 과정에서 생성중인 테이블이니 어차피 접근에 대한 염려를 하지 않아도 된다.
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	// TODO 번역 : 파일에 대해 수정된 내용을 스토리지에 반영해야합니다.
	// : writeback all the modified contents to the storage.

	//hash_destroy(&spt->main_table, destructHashTable);
	// hash_destroy에 순회하면서 다 빼는거 포함 되어 있음. 먹일 콜백 함수를 writeback을 적용 하는 내용으로 정의해야해요
	//free(&spt->main_table);
}


bool hash_less_standard(struct hash_elem *A, struct hash_elem *B)
{
	struct page *pageA = hash_entry(A, struct page, page_hashelem);
	struct page *pageB = hash_entry(B, struct page, page_hashelem);
	return pageA->va > pageB->va;
}

void destructHashTable(struct hash_elem *he, void* aux)
{
	// Boom the spt!!
	// 테이블에 있는 아이템을 하나씩 끝장낸다.
	// File을 갖고 있는 경우.. writeback을 모두 적용시켜놔야한다. 즉, swap out 다 때려줘야함
}
