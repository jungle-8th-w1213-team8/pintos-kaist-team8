/* vm.c: 가상 메모리 객체를 위한 일반(generic) 인터페이스입니다. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* 각 서브시스템의 초기화 코드를 호출하여 가상 메모리 서브시스템을 초기화합니다. */
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

/* 페이지의 타입을 가져옵니다. 이 함수는 페이지가 초기화된 후에
 * 해당 페이지의 타입을 알고 싶을 때 유용합니다.
 * 이 함수는 현재 완전히 구현되어 있습니다. */
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

/* 초기화 함수를 사용하여 대기 중인(pending) 페이지 오브젝트를 생성합니다.
 * 페이지를 생성 시 직접 생성하지 말고,
 * 이 함수 또는 `vm_alloc_page`를 통해 생성해야 합니다. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/** TODO: 페이지를 생성하고, VM의 타입에 따라 초기화 함수를 가져온(fetch) 뒤,
		 *     uninit_new를 호출하여 "uninit" 페이지 구조체를 생성하세요.
		 *     일단 uninit_new를 호출한 후에는 해당 필드를 수정해야 합니다. */

		/** TODO: 페이지를 spt에 삽입하세요. */
	}
err:
	return false;
}

/* `spt`에서 VA를 찾아 페이지를 반환합니다. 오류 발생 시 NULL을 반환합니다. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */

	return page;
}

/* Insert PAGE into spt with validation. */
/* 유효성 검사를 통해 PAGE를 spt에 삽입합니다. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
/* 교체(evict)될 프레임 구조체(struct frame)를 가져옵니다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* 한 페이지를 교체(evict)하고 해당 프레임을 반환합니다.
 * 오류 발생 시 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc()로 프레임을 가져옵니다. 
 * 사용 가능한 페이지가 없으면, 페이지를 교체(evict)하여 반환합니다. 
 * 이 함수는 항상 유효한 주소를 반환합니다. 
 * 즉, 사용자 풀(user pool) 메모리가 가득 찬 경우, 
 * 프레임을 교체하여 사용 가능한 메모리 공간을 확보하는 것입니다.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
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

/* Claim the page that allocate on VA. 
 * VA에 할당된 페이지를 소유(claim)합니다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/** TODO: 페이지의 VA(가상 주소)를 프레임의 PA(물리 주소)에 매핑하는 
	 *        페이지 테이블 엔트리를 삽입. */

	return swap_in (page, frame->kva);
}

/* 새 supplemental page table을 초기화. */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
}

/* Supplemental page table을 src → dst로 복사. */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Supplemental page table이 보유(hold)했던 자원을 해제. */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/** TODO: 해당 스레드가 보유(hold)했던 모든 supplemental_page_table을 파괴하고
	 *        수정된 모든 내용을 스토리지에 기록(writeback)하세요. */
}
