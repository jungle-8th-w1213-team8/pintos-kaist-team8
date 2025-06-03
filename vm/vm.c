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

	lock_init(&g_frame_lock);
	list_init(&g_frame_table);
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

/* 유틸, 헬퍼 ~ */
static inline bool is_target_stack(void* rsp, void* addr) {
    return addr != NULL
        && addr >= rsp - STACK_MAX_GAP
        && addr >= (void *)(USER_STACK - STACK_MAX_SIZE)
        && addr < (void *)USER_STACK;
}

static void vm_free_frame(struct frame *frame) {
	ASSERT(frame != NULL);

	lock_acquire(&g_frame_lock);
	
	list_remove(&frame->f_elem); // 프레임 테이블로부터 제거
	palloc_free_page(frame->kva); // 실제 프레임을 제거
	free(frame); // 할당했던 메모리 free 

	lock_release(&g_frame_lock);
}

unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry(p_, struct page, page_hashelem);
	return hash_bytes(&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED
){
	const struct page *a = hash_entry(a_, struct page, page_hashelem);
	const struct page *b = hash_entry(b_, struct page, page_hashelem);
	return a->va > b->va;
}

/* ~ 유틸, 헬퍼 */

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, //새로운 uninit 페이지 생성 + uninit_new()로 
		vm_initializer *init, void *aux) {
	// 참고: 여기서 enum vm_type type란, 얘가 미래에 될 타입.
	ASSERT (VM_TYPE(type) != VM_UNINIT); 

	struct supplemental_page_table *spt = &thread_current ()->spt; //초기화 SPT에 등록(중복 방지)

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) { //SPT(해시 테이블)에서 VA로 페이지 찾기
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		 
		// 새 페이지를 콜록
		struct page *page = (struct page *)calloc(1, sizeof(struct page));
		if (page == NULL)
			goto err;

		// 필드들 초기화
		bool (*page_initializer)(struct page *, enum vm_type, void *);
		switch (VM_TYPE(type)) {
		case VM_ANON:
			page_initializer = anon_initializer;
			uninit_new(page, upage, init, type, aux, page_initializer);
			break;
		case VM_FILE:
			page_initializer = file_backed_initializer;
			uninit_new(page, upage, init, type, aux, page_initializer);
			break;
		default:
			printf("정의되지 않은 VM_TYPE(type)!\n");
			goto err;
		}
		page->writable = writable;

		// hash_insert 대신 spt_insert_page 사용하게끔 수정 :
		bool is_inserted = spt_insert_page(spt, page);
		if (!is_inserted) {
			free (page);
			goto err;
		}

		return is_inserted;  /* success means “page reserved, no frame yet” */
	}
	/* 중복 페이지 생성 방지 */
	goto err;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va){
    ASSERT (spt != NULL);
	
    /* VA의 field set 기반으로 더미 struct page를 만듦 */
	struct page* page_p = (struct page *)malloc(sizeof(struct page));
	struct page* found_page = NULL;
    page_p->va = pg_round_down (va); // 페이지 경계에 맞도록 조정

    /* 해시 테이블을 조회 */
    struct hash_elem *e = hash_find (&spt->main_table, &page_p->page_hashelem);
    if (e != NULL){
		found_page = hash_entry (e, struct page, page_hashelem);
	}
	free(page_p);
    return found_page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt ,struct page *page) { //페이지를 SPT에 삽입(중복방지)
	int succ = true;
	// hash_insert 함수를 사용해서 spt에 넣을 물건 page를 던진다.
	// hash_insert는 적절한 위치를 탐색하고, 중복 탐색도 합니다. (중복은 삽입 안됩니다!)
	// insert_elem으로 삽입에 대한 핵심 로직이 이루어집니다.
	struct hash_elem *result = hash_insert(&spt->main_table, &page->page_hashelem);
	if(result != NULL) succ = false;
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
	lock_acquire(&g_frame_lock);
	void *kva = palloc_get_page(PAL_USER);

	/* 할당 실패 시 eviction policy 집행 */
	if (kva == NULL) {
		// TODO: evict 대상 프레임 선택, swap out, 프레임 재활용.
		lock_release(&g_frame_lock);
		PANIC("Out of user memory and no eviction implemented yet.");
	}
	struct frame *frame = palloc_get_page(PAL_USER);

	if(frame == NULL) {
		palloc_free_page(kva);
		lock_release(&g_frame_lock);
		PANIC("struct frame 할당 실패!");
	}

	frame->kva = kva;
	frame->page = NULL;

	// 전역 frame table에 등록
	list_push_back(&g_frame_table, &frame->f_elem);

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	lock_release(&g_frame_lock);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	// 이 함수의 행위 책임 :
		// 스택 성장 처리 (ok)
		// 스택 성장 필요 유무를 확인 (?)
		void *new_page_addr = pg_round_down(addr);

		 /* 스택은 익명 페이지로 할당 (VM_ANON) */
		 bool success = vm_alloc_page_with_initializer(
			VM_ANON,        // 타입: 익명 페이지
			new_page_addr,  // 페이지의 가상주소
			true,           // 쓰기 가능
			NULL,           // 초기화 함수 (필요 없음)
			NULL            // aux 데이터 (필요 없음)
		);
	  
		if (!success)
		  PANIC("vm_stack_growth 실패! 메모리 부족?");
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
	// 쓰기 제한이 걸린 페이지를 다뤄야해요
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, 
						void *addr UNUSED,
						bool user UNUSED, 
						bool write UNUSED, 
						bool not_present UNUSED) 
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;

	// 얼리 리턴
	// 아! 커널 쓰레드는 page fault 날 일 자체가 없다!
	if (addr == NULL || is_kernel_vaddr(addr) | !not_present)
		return false;
	
	/* TODO: Validate the fault */
	// todo: 페이지 폴트가 스택 확장에 대한 유효한 경우인지를 확인해야 합니다.
	void *rsp = f->rsp; // user access인 경우 rsp는 유저 stack을 가리킨다.
	if (!user)			// kernel access인 경우 thread에서 rsp를 가져와야 한다.
		rsp = thread_current()->rsp;

	page = spt_find_page(spt, addr);
	// 페이지가 SPT에 없음
	if (page == NULL) {
		// 스택 확장으로 처리할 수 있는 폴트인 경우
		if (is_target_stack(rsp,addr)){
			// vm_stack_growth()로 스택을 확장
			vm_stack_growth(addr);
			// 새 페이지를 얻어 claim
			page = spt_find_page(spt, addr);
			if (page != NULL)
				return vm_do_claim_page(page);
			else
        		return false;
		}else{
			// 스택 확장으로 안되는 건 어쩔 수 없다.
			return false;
		}
	}
	
	if (write == 1 && page->writable == 0) // write 불가능한 페이지에 write를 요청함
		return false;

	return vm_do_claim_page(page);
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
	struct supplemental_page_table *spt = &thread_current()->spt;
	page = spt_find_page(spt, va);
	if (page == NULL)
		return false;  // SPT에 없는 경우

	return vm_do_claim_page(page); // 프레임 할당, 내용 초기화, PTE 설정 등
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	ASSERT (page != NULL);
	struct frame *frame = vm_get_frame ();
	if (frame == NULL)
		return false;

	// mmu.c에는 pml4에 대한 내용을 다루고 있음, pml4에 실제로 올라가는 내용을 다루기

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* PML4 하드웨어에 등록 */
	uint64_t *pml4 = thread_current()->pml4;
	void *upage = page->va;
	void *kpage = frame->kva;
	bool rw = page->writable;

	// printf("vm_do_claim_page()에서 - %p,%p,%p,%d. \n",pml4, upage, kpage, rw);
	bool is_page_set = pml4_set_page(pml4, upage, kpage, rw);

	if (!is_page_set) {
		// 매핑 실패 시 바로 원상복구
		// printf("pml4_set_page 실패!!");
		page->frame = NULL;
		frame->page = NULL;
		vm_free_frame(frame);
		return false;
	}

	// printf("vm_do_claim_page()의 pml4_set_page 결과 - %d\n",is_page_set);
	bool is_swapped_in = swap_in(page, frame->kva);
	// printf("vm_do_claim_page()의 swap_in 결과 - %d\n",is_swapped_in);
	return is_swapped_in;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt ) { //SPT 해시 테이블 초기화
	hash_init(&spt->main_table, page_hash, page_less, NULL);
	// lock_init(&spt->spt_lock);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src ) {
	// src에서 dst로 supplemental_page_table 복사하기.
	
	if(hash_empty(&src->main_table)) return true; // 복사할게 없네용 : true 반환
	struct hash_iterator i;
	hash_first (&i, &src->main_table);
	while (hash_next (&i))
	{
		struct page *srcPage = hash_entry(hash_cur (&i), struct page, page_hashelem);
		enum vm_type type = page_get_type(srcPage);
		void *upage = srcPage->va;
		bool writable = srcPage->writable;

		// VM_UNINIT 쪽 삽입을 허용을 안하는데 이 쪽 분기를 타겠냐고~
		if(type == VM_UNINIT)
		{
			//vm_initializer *init = srcPage->uninit.init;
			//void *aux = srcPage->uninit.aux;
		}
		else if(type == VM_ANON)
		{
			// 실제 작동 여기만 함 . 딴데 보지 마세요
			if(!vm_alloc_page(type, upage, writable)) return false;
			if(!vm_claim_page(upage)) return false;
			struct page *newPage = spt_find_page(dst, upage);
			if(srcPage->frame != NULL)
				memcpy(newPage->frame->kva, srcPage->frame->kva, PGSIZE);
			
		}
		else if(type == VM_FILE)
		{
			//vm_initializer *init = srcPage->file;
			//void *aux = srcPage->file;
			// if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
			// {
			// 	return false;
			// }
			// else
			// {
			// 	if(!vm_claim_page(upage)) return false;
			// 	struct page *newPage = spt_find_page(dst, upage);
			// 	memcpy(newPage->frame->kva, srcPage->frame->kva, PGSIZE);
			// }
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	hash_clear(&spt->main_table, NULL);
	// temp. 좀더 정성껏 작성 할 것. 특히 파일 있는경우..
}