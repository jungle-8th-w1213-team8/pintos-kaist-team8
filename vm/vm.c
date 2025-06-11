// /* vm.c: Generic interface for virtual memory objects. */

// #include "threads/malloc.h"
// #include "vm/vm.h"
// #include "vm/inspect.h"
// #include "kernel/hash.h"

// #include "threads/vaddr.h"
// #include "userprog/process.h"
// #include "threads/mmu.h"

// /* Initializes the virtual memory subsystem by invoking each subsystem's
//  * intialize codes. */
// void
// vm_init (void) {
// 	vm_anon_init ();
// 	vm_file_init ();
// #ifdef EFILESYS  /* For project 4 */
// 	pagecache_init ();
// #endif
// 	register_inspect_intr ();
// 	/* DO NOT MODIFY UPPER LINES. */
// 	/* TODO: Your code goes here. */

// 	lock_init(&g_frame_lock);
// 	list_init(&g_frame_table);
// }

// /* Get the type of the page. This function is useful if you want to know the
//  * type of the page after it will be initialized.
//  * This function is fully implemented now. */
// enum vm_type
// page_get_type (struct page *page) {
// 	int ty = VM_TYPE (page->operations->type);
// 	switch (ty) {
// 		case VM_UNINIT:
// 			return VM_TYPE (page->uninit.type);
// 		default:
// 			return ty;
// 	}
// }

// /* Helpers */
// static struct frame *vm_get_victim (void);
// static bool vm_do_claim_page (struct page *page);
// static struct frame *vm_evict_frame (void);

// /* 유틸, 헬퍼 ~ */
// static inline bool is_target_stack(void* rsp, void* addr) {
//     return addr != NULL
//         && addr >= rsp - STACK_MAX_GAP
//         && addr >= (void *)(USER_STACK - STACK_MAX_SIZE)
//         && addr < (void *)USER_STACK;
// }

// static void vm_free_frame(struct frame *frame) {
// 	ASSERT(frame != NULL);

// 	lock_acquire(&g_frame_lock);
	
// 	list_remove(&frame->f_elem); // 프레임 테이블로부터 제거
// 	palloc_free_page(frame->kva); // 실제 프레임을 제거
// 	free(frame); // 할당했던 메모리 free 

// 	lock_release(&g_frame_lock);
// }

// unsigned page_hash(const struct hash_elem *p_, void *aux)
// {
// 	const struct page *p = hash_entry(p_, struct page, page_hashelem);
// 	return hash_bytes(&p->va, sizeof p->va);
// }

// /* Returns true if page a precedes page b. */
// bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux)
// {
// 	const struct page *a = hash_entry(a_, struct page, page_hashelem);
// 	const struct page *b = hash_entry(b_, struct page, page_hashelem);
// 	return a->va > b->va;
// }

// /* Create the pending page object with initializer. If you want to create a
//  * page, do not create it directly and make it through this function or
//  * `vm_alloc_page`. */
// bool
// vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux)\
// {
// 	ASSERT (VM_TYPE(type) != VM_UNINIT); 
// 	struct supplemental_page_table *spt = &thread_current ()->spt;

// 	if (spt_find_page (spt, upage) == NULL)
// 	{
// 		bool (*page_initializer)(struct page *, enum vm_type, void *kva);
// 		struct page *page = (struct page *)calloc(1, sizeof(struct page));

// 		switch (VM_TYPE(type)) {
// 		case VM_ANON:
// 			page_initializer = anon_initializer;
// 			break;
// 		case VM_FILE:
// 			page_initializer = file_backed_initializer;
// 			break;
// 		default:
// 			free(page);
// 			printf("정의되지 않은 VM_TYPE(type)!\n");
// 			goto err;
// 		}
// 		uninit_new(page, upage, init, type, aux, page_initializer);
// 		page->writable = writable;

// 		bool is_inserted = spt_insert_page(spt, page);
// 		if (!is_inserted) {
// 			free (page);
// 			goto err;
// 		}

// 		return is_inserted;  // 페이지가 SPT에 등록되었으나, 프레임을 배정받지 못해 직접적인 메모리 등록은 아직 이루어지지 않았습니다.
// 	}
// 	goto err;
// err:
// 	return false;
// }

// /* Find VA from spt and return page. On error, return NULL. */
// struct page * spt_find_page (struct supplemental_page_table *spt, void *va){
//     ASSERT (spt != NULL);
// 	if(va == NULL) return NULL;
	
//     /* VA의 field set 기반으로 더미 struct page를 만듦 */
// 	struct page* page_p = (struct page *)malloc(sizeof(struct page));
// 	struct page* found_page = NULL;
//     page_p->va = pg_round_down (va); // 페이지 경계에 맞도록 조정후 개시

//     /* 해시 테이블을 조회 */
//     struct hash_elem *e = hash_find (&spt->main_table, &page_p->page_hashelem);
//     if (e != NULL){
// 		found_page = hash_entry (e, struct page, page_hashelem);
// 	}
// 	free(page_p);
//     return found_page;
// }

// /* Insert PAGE into spt with validation. */
// bool
// spt_insert_page (struct supplemental_page_table *spt ,struct page *page) { //페이지를 SPT에 삽입(중복방지)
// 	int succ = true;
// 	struct hash_elem *result = hash_insert(&spt->main_table, &page->page_hashelem);
// 	if(result != NULL) succ = false;
// 	return succ;
// }

// void
// spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
// 	hash_delete(&spt->main_table, &page->page_hashelem);

// 	// vm_dealloc_page (page);
// 	return true;
// }

// /* Get the struct frame, that will be evicted. */
// static struct frame * vm_get_victim (void) {
// 	struct frame *victim = NULL;
// 	 /* TODO: The policy for eviction is up to you. */
//     struct thread *curr = thread_current();
//     struct list_elem *now = list_begin(&g_frame_table);
    
//     lock_acquire(&g_frame_lock);
//     for (; now != list_end(&g_frame_table); now = list_next(now)) {
//         victim = list_entry(now, struct frame, f_elem);

//         if (pml4_is_accessed(curr->pml4, victim->page->va)) {
//             pml4_set_accessed(curr->pml4, victim->page->va, 0);
//         } else {
//             lock_release(&g_frame_lock);
//             return victim;
//         }
//     }

//     for (; now != list_end(&g_frame_table); now = list_next(now)) {
//         victim = list_entry(now, struct frame, f_elem);

//         if (pml4_is_accessed(curr->pml4, victim->page->va)) {
//             pml4_set_accessed(curr->pml4, victim->page->va, 0);
//         } else {
//             lock_release(&g_frame_lock);
//             return victim;
//         }
//     }
    
//     lock_release(&g_frame_lock);
//     ASSERT(now != NULL);
//     return victim;
// }

// /* Evict one page and return the corresponding frame.
//  * Return NULL on error.*/
// static struct frame* vm_evict_frame (void) {
// 	struct frame *victim  = vm_get_victim();
// 	if(victim==NULL) return NULL;	

// 	struct page *page =victim->page;
// 	if (page) {
// 		if (!swap_out(page))
// 			return NULL;
// 		pml4_clear_page(thread_current()->pml4, page->va);
// 		// list_remove(&page->frame->frame_elem);

// 		page->frame = NULL; // 연결 해제

// 		return victim;
// 	}
// 	return NULL;

// }


// /* palloc()을 호출하고 프레임을 얻습니다. 사용 가능한 페이지가 없으면 페이지를 
//  * 축출(evict)하고 반환합니다. 이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀
//  * 메모리가 가득 차면, 이 함수는 프레임을 축출하여 사용 가능한 메모리 공간을 확보합니다.*/
// static struct frame* vm_get_frame (void) {
// 	struct frame *frame = malloc(sizeof(struct frame));
// 	ASSERT(frame!=NULL);
// 	frame->r_cnt=0;

// 	frame->kva= palloc_get_page(PAL_USER | PAL_ZERO);
// 	if(frame->kva==NULL){
// 		struct frame * victim=vm_evict_frame(); //이 안에서 swap out
// 		ASSERT(victim!=NULL);
// 		frame->kva=victim->kva;
		
// 		free(victim );
// 	}
// 	frame->page=NULL;

// 	list_push_back(&g_frame_table, &frame->f_elem);
	
// 	ASSERT(frame != NULL);
// 	ASSERT(frame->page == NULL);
// 	ASSERT(frame->kva!=NULL);
// 	return frame;
// }

// /* Growing the stack. */
// static void
// vm_stack_growth (void *addr) {
// 	void *new_page_addr = pg_round_down(addr);
// 	vm_alloc_page(VM_ANON, new_page_addr, true);
// }

// /* Handle the fault on write_protected page */
// static bool
// vm_handle_wp (struct page *page) {
	
// 	void * old_kva= page->frame->kva;
// 	page->frame->r_cnt--;

// 	struct frame * frame=vm_get_frame();
// 	page->frame=frame;
// 	frame->page=page;

// 	frame->r_cnt++;

	
// 	if(!pml4_set_page(thread_current()->pml4, page->va, frame->kva, true)){
// 		PANIC("TODO");
// 	}
// 	memcpy(page->frame->kva, old_kva, PGSIZE);
// 	return true;
// }

// /* Return true on success */
// bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user , bool write, bool not_present) 
// {

// 	// ASSERT(addr!=NULL);
//     if (!is_user_vaddr(addr)) return false;

//     struct supplemental_page_table *spt = &thread_current()->spt;
// 	// addr = pg_round_down(addr);
//     struct page *page = spt_find_page(spt, addr);
// 	uintptr_t rsp = thread_current()->rsp; // 유저 스택의 rsp 가져오기

// 	if (page && write && !not_present) {
//         if (!page->writable) {  
//             return false;     
//         }
//         return vm_handle_wp(page);
//     }

//     if (page && !write && !not_present) return false;


// 	if(page){
// 		return vm_do_claim_page(page);
// 	}

//     if (page == NULL) {
//         if (is_target_stack(rsp, addr))
// 		{
//             vm_stack_growth(pg_round_down(addr));
// 			return true;
// 		}
//         return false;
// 	}
// }


// /* Free the page.
//  * DO NOT MODIFY THIS FUNCTION. */
// void
// vm_dealloc_page (struct page *page) {
// 	destroy (page);
// 	free (page);
// }

// /* Claim the page that allocate on VA. */
// bool
// vm_claim_page (void *va) {
// 	struct page *page = spt_find_page(&thread_current()->spt, va);
// 	if (page == NULL) return false;
// 	return vm_do_claim_page(page);
// }

// /* Claim the PAGE and set up the mmu. */
// static bool
// vm_do_claim_page (struct page *page) {
// 	struct frame *frame = vm_get_frame ();
// 	if (frame == NULL) return false;
// 	/* Set links */
// 	frame->page = page;
// 	page->frame = frame;
// 	frame->r_cnt++;

// 	bool is_page_set = pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);

// 	// printf("vm_do_claim_page()의 pml4_set_page 결과 - %d\n",is_page_set);
// 	bool is_swapped_in = swap_in(page, frame->kva);
// 	// printf("vm_do_claim_page()의 swap_in 결과 - %d\n",is_swapped_in);
// 	return is_swapped_in;
// }

// /* Initialize new supplemental page table */
// void
// supplemental_page_table_init (struct supplemental_page_table *spt )
// {
// 	hash_init(&spt->main_table, page_hash, page_less, NULL);
// }

// static void *duplicate_aux(struct page *src_page, enum vm_type type)
// {
   
// 	struct file_info *src_info;
//     if (type == VM_UNINIT)
//         src_info = (struct file_info *)src_page->uninit.aux;
//     else if (type == VM_FILE)
//         src_info = (struct file_info *)src_page->file.aux;
//     else
//         return NULL; // 처리 불가

//     struct file_info *dst_info = malloc(sizeof(struct file_info));

//     dst_info->file = file_reopen(src_info->file);
//     dst_info->ofs = src_info->ofs;
//     dst_info->upage = src_info->upage;
//     dst_info->read_bytes = src_info->read_bytes;
//     dst_info->zero_bytes = src_info->zero_bytes;
//     dst_info->writable = src_info->writable;
// 	dst_info->mmap_length = src_info->mmap_length;
//     return dst_info;
   
// }

// bool page_table_copy(struct page* src_page, void *va){
// 	struct page *page= spt_find_page(&thread_current()->spt, va);
// 	if(page==NULL) return false;

// 	if(page->frame == NULL){
// 		page->frame=src_page->frame;
// 		page->writable=src_page->writable;
// 		src_page->frame->r_cnt++;
		
// 	}

// 	if(!pml4_set_page(thread_current()->pml4, page->va, src_page->frame->kva, false)){
// 		PANIC("TODO");
// 	}

// 	return swap_in(page, src_page->frame->kva);
// }

// /* Copy supplemental page table from src to dst */
// bool supplemental_page_table_copy(struct supplemental_page_table *dst , struct supplemental_page_table *src )
// {
// 	struct hash_iterator i;
// 	hash_first(&i, &src->main_table);
// 	struct thread *cur = thread_current();
 
// 	while (hash_next(&i))
// 	{
// 	   // src_page 정보
// 	   struct page *src_page = hash_entry(hash_cur(&i), struct page, page_hashelem);
// 	   enum vm_type type = src_page->operations->type;
// 	   void *upage = src_page->va;
// 	   bool writable = src_page->writable;
 
// 	   /* 1) type이 uninit이면 */
// 	   if (type == VM_UNINIT)
// 	   { // 부모의 예약된 타입을 가져옴 
// 		  enum vm_type reserved_type = src_page->uninit.type;
// 		  vm_initializer *init = src_page->uninit.init;
// 		  void *aux = duplicate_aux(src_page,VM_UNINIT);
// 		  ASSERT(aux!=NULL);
		  
// 		  if(!vm_alloc_page_with_initializer(reserved_type, upage, writable, init, aux))
// 			  return false;
// 		  continue;
// 	   }
 
// 	   /* 2) type이 file-backed이면 */
// 	   else if (type == VM_FILE)
// 	   {
	   
// 		   struct file_info *aux = duplicate_aux(src_page, VM_FILE);
// 		   ASSERT(aux!=NULL);
 
// 		   if (!vm_alloc_page_with_initializer(type, upage, writable, lazy_load_segment, aux))
// 			   return false;
	   
// 		   if (!vm_claim_page(upage))
// 			   return false;
	   
// 		   continue;
// 	   }
// 	   else if(type==VM_ANON){
// 		   // 3. type이 anon 이면 
// 		   if (!vm_alloc_page(type, upage, writable)) // uninit page 생성 & 초기화
// 			  // init(lazy_load_segment)는 page_fault가 발생할때 호출됨
// 			  // 지금 만드는 페이지는 page_fault가 일어날 때까지 기다리지 않고 바로 내용을 넣어줘야 하므로 필요 없음
// 			  return false;
		 
// 			 if(!page_table_copy(src_page, upage))
// 				 return false;
 
// 			 // 매핑된 프레임에 내용 로딩
// 			 struct page *dst_page = spt_find_page(dst, upage);
// 			 if(src_page->frame != NULL)
// 				 memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
// 	 }
// 	   else{
// 		 return false;
// 	   }
 
 
// 	}
// 	 return true;
// }


// void page_desturctor(struct hash_elem *e, void * aux){
// 	struct page *page = hash_entry(e, struct page, page_hashelem);
//     if (page->operations->destroy != NULL) {
//         vm_dealloc_page(page);
//     }
// }


// /* Free the resource hold by the supplemental page table */
// void supplemental_page_table_kill(struct supplemental_page_table *spt)
// {
// 	hash_clear(&spt->main_table, page_desturctor);
// }

/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "userprog/process.h"
#define STACK_GROW_RANGE 4192
struct frame_table *frame_table;

/* 각 서브시스템의 초기화 코드를 호출하여 가상 메모리 서브시스템을 초기화합니다. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* 이 위쪽은 수정하지 마세요 !! */
	/* TODO: 이 아래쪽부터 코드를 추가하세요 */
	list_init(&g_frame_table);

}

/* 페이지의 타입을 가져옵니다. 이 함수는 페이지가 초기화된 후 타입을 알고 싶을 때 유용합니다.
 * 이 함수는 이미 완전히 구현되어 있습니다. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

void frame_table_init(){
	frame_table = malloc(sizeof(struct frame_table));
	list_init(&frame_table->frame_list);
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* 초기화 함수와 함께 대기 중인 페이지 객체를 생성합니다. 페이지를 직접 생성하지 말고,
 * 반드시 이 함수나 `vm_alloc_page`를 통해 생성하세요. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	struct supplemental_page_table *spt = &thread_current()->spt;
	ASSERT(spt!=NULL);

	/* 이미 해당 page가 SPT에 존재하는지 확인합니다 */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: VM 타입에 따라 페이지를 생성하고, 초기화 함수를 가져온 뒤,
		 * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 생성하세요.
		 * TODO: uninit_new 호출 후에는 필요한 필드를 수정해야 합니다. */
		bool (*page_initializer)(struct page *, enum vm_type, void *kva);
		struct page *page = malloc(sizeof(struct page));
		ASSERT(page!=NULL);

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			page_initializer = anon_initializer;
			break;
		case VM_FILE:
			page_initializer = file_backed_initializer;
			break;
		default:
			free(page);
			goto err;
			break;
		}

		uninit_new(page, upage, init, type, aux, page_initializer);
		page->writable=writable;
		/* TODO: 생성한 페이지를 spt에 삽입하세요. */
		if (!spt_insert_page(spt, page))
		{
		   // 실패 시 메모리 누수 방지 위해 free
		   free(page);
		   // 실패 했으니까 에러로 가야겠지?
		   goto err;
		}
  
		return true;
		
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* 가상 주소를 통해 SPT에서 페이지를 찾아 리턴합니다.
 * 에러가 발생하면 NULL을 리턴하세요 */
struct page *
spt_find_page(struct supplemental_page_table *spt, void *va)
{
    ASSERT(spt != NULL);
    // ASSERT(va != NULL);
	if(va==NULL) return NULL;

	struct page temp;
	temp.va = pg_round_down(va);
	
	struct hash_elem *e = hash_find(&spt->main_table, &temp.page_hashelem);
	
	if (e == NULL)
		return NULL;
	else
		return hash_entry(e, struct page, page_hashelem);
	
}


/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt,
					 struct page *page)
{
	int succ = false;
	ASSERT(page!=NULL);
	struct hash_elem * e=hash_insert(&spt->main_table, &page->page_hashelem);
	if(e!=NULL) return succ; //실패했음

	succ=true;
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	hash_delete(&spt->main_table, &page->page_hashelem);	
	// vm_dealloc_page(page); //<< 이거 쓰면 swap out~->swap in이 안될 것 같은데? 

}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim;
	/* TODO: 교체 정책을 여기서 구현해서 희생자 페이지 찾기 */

	victim = list_entry(list_pop_front(&g_frame_table), struct frame, f_elem);
	ASSERT(victim!=NULL);

	return victim;
}

/* 한 페이지를 교체(evict)하고 해당 프레임을 반환합니다.
 * 에러가 발생하면 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim  = vm_get_victim();
	if(victim==NULL) return NULL;	

	struct page *page =victim->page;
	if (page) {
		if (!swap_out(page))
			return NULL;
		pml4_clear_page(thread_current()->pml4, page->va);
		// list_remove(&page->frame->frame_elem);

		page->frame = NULL; // 연결 해제

		return victim;
	}
	return NULL;

}

/* palloc()을 사용하여 프레임을 할당합니다.
 * 사용 가능한 페이지가 없으면 페이지를 교체(evict)하여 반환합니다.
 * 이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀 메모리가 가득 차면,
 * 이 함수는 프레임을 교체하여 사용 가능한 메모리 공간을 확보합니다.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = malloc(sizeof(struct frame));
	ASSERT(frame!=NULL);
	frame->r_cnt=0;

	frame->kva= palloc_get_page(PAL_USER | PAL_ZERO);
	if(frame->kva==NULL){
		struct frame * victim=vm_evict_frame(); //이 안에서 swap out
		ASSERT(victim!=NULL);
		frame->kva=victim->kva;
		
		free(victim );
	}
	frame->page=NULL;

	list_push_back(&g_frame_table, &frame->f_elem);
	
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	ASSERT(frame->kva!=NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr)
{
	/* 스택 최하단에 익명 페이지를 추가하여 사용
	 * addr은 PGSIZE로 내림(정렬)하여 사용	 */
	vm_alloc_page(VM_ANON, addr, true); // 스택 최하단에 익명 페이지 추가
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page)
{
	
	void * old_kva= page->frame->kva;
	page->frame->r_cnt--;

	struct frame * frame=vm_get_frame();
	page->frame=frame;
	frame->page=page;

	frame->r_cnt++;

	
	if(!pml4_set_page(thread_current()->pml4, page->va, frame->kva, true)){
		PANIC("TODO");
	}
	memcpy(page->frame->kva, old_kva, PGSIZE);

	return true;
}

/* Return true on success */
/* 인터럽트 프레임, addr=폴트를 일으킨 주소(코드일 수도있고 데이터일수도 있음),
user=사용자 접근인지 커널 접근인지, write=true면 쓰기 허용 false면 읽기만
not_present: true면 존재하지 않는 페이지, false면 권한없어서 페이지 폴트 에러  */
bool vm_try_handle_fault(struct intr_frame *f , void *addr ,
						 bool user UNUSED, bool write , bool not_present )
{

	// ASSERT(addr!=NULL);
    if (!is_user_vaddr(addr)) return false;

    struct supplemental_page_table *spt = &thread_current()->spt;
	// addr = pg_round_down(addr);
    struct page *page = spt_find_page(spt, addr);
	uintptr_t rsp = thread_current()->rsp; // 유저 스택의 rsp 가져오기

	if (page && write && !not_present) {
        if (!page->writable) {  
            return false;     
        }
        return vm_handle_wp(page);
    }

    if (page && !write && !not_present) {
        return false;
    }


	if(page){
		return vm_do_claim_page(page);
	}

    if (page == NULL) {
        if (addr > rsp - PGSIZE && addr >= USER_STACK - (1 << 20) && addr < USER_STACK) {
            vm_stack_growth(pg_round_down(addr));
			return true;
		}
        
        return false;
	}
}

/* Free the page.
프레임 해제, 파일 wriet-back, 페이지 테이블 매핑 해제 등 모든 자원 정리 수행 
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page); 
}

/* VA에 할당된 페이지를 요구합니다 . */
bool vm_claim_page(void *va)
{
	struct page *page = spt_find_page(&thread_current()->spt, va);
	/* TODO: Fill this function */
	if(page==NULL) return false;

	return vm_do_claim_page(page);
}

/* PAGE를 요구하고 mmu를 설정합니다*/
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	
	/* Set links */
	frame->page = page;
	page->frame = frame;
	
	frame->r_cnt++;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if(!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)){
		// swap-out? 
		PANIC("TODO");
	}
	

	return swap_in(page, frame->kva);
}

bool is_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	if(a==NULL) return true;
	else if (b==NULL) return true;
	const struct page *page_a=hash_entry(a, struct page, page_hashelem);
	const struct page *page_b=hash_entry(b, struct page, page_hashelem);
	return page_a->va < page_b->va;
}

size_t page_hash(const struct hash_elem *e, void * aux){
	const struct page *p = hash_entry(e, struct page, page_hashelem);
	return hash_bytes(&p->va, sizeof p->va);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt)
{
	if(!hash_init(&spt->main_table, page_hash, is_less, NULL))
		return;
}


static void *duplicate_aux(struct page *src_page, enum vm_type type)
{
   
	struct file_info *src_info;
    if (type == VM_UNINIT)
        src_info = (struct file_info *)src_page->uninit.aux;
    else if (type == VM_FILE)
        src_info = (struct file_info *)src_page->file.aux;
    else
        return NULL; // 처리 불가

    struct file_info *dst_info = malloc(sizeof(struct file_info));

    dst_info->file = file_reopen(src_info->file);
    dst_info->ofs = src_info->ofs;
    dst_info->upage = src_info->upage;
    dst_info->read_bytes = src_info->read_bytes;
    dst_info->zero_bytes = src_info->zero_bytes;
    dst_info->writable = src_info->writable;
    dst_info->mmap_length = src_info->mmap_length;
    return dst_info;
   
}

bool page_table_copy(struct page* src_page, void *va){
	struct page *page= spt_find_page(&thread_current()->spt, va);
	if(page==NULL) return false;

	if(page->frame == NULL){
		page->frame=src_page->frame;
		page->writable=src_page->writable;
		src_page->frame->r_cnt++;
		
	}

	if(!pml4_set_page(thread_current()->pml4, page->va, src_page->frame->kva, false)){
		PANIC("TODO");
	}

	return swap_in(page, src_page->frame->kva);
}

bool supplemental_page_table_copy(struct supplemental_page_table *dst , struct supplemental_page_table *src )
{
   struct hash_iterator i;
   hash_first(&i, &src->main_table);
   struct thread *cur = thread_current();

   while (hash_next(&i))
   {
      // src_page 정보
      struct page *src_page = hash_entry(hash_cur(&i), struct page, page_hashelem);
      enum vm_type type = src_page->operations->type;
      void *upage = src_page->va;
      bool writable = src_page->writable;

      /* 1) type이 uninit이면 */
      if (type == VM_UNINIT)
      { // 부모의 예약된 타입을 가져옴 
		 enum vm_type reserved_type = src_page->uninit.type;
         vm_initializer *init = src_page->uninit.init;
         void *aux = duplicate_aux(src_page,VM_UNINIT);
		 ASSERT(aux!=NULL);
		 
         if(!vm_alloc_page_with_initializer(reserved_type, upage, writable, init, aux))
		 	return false;
         continue;
      }

      /* 2) type이 file-backed이면 */
	  else if (type == VM_FILE)
	  {
	  
		  struct file_info *aux = duplicate_aux(src_page, VM_FILE);
		  ASSERT(aux!=NULL);

		  if (!vm_alloc_page_with_initializer(type, upage, writable, lazy_load_segment, aux))
			  return false;
	  
		  if (!vm_claim_page(upage))
			  return false;
	  
		  continue;
	  }
	  else if(type==VM_ANON){
		  // 3. type이 anon 이면 
		  if (!vm_alloc_page(type, upage, writable)) // uninit page 생성 & 초기화
			 // init(lazy_load_segment)는 page_fault가 발생할때 호출됨
			 // 지금 만드는 페이지는 page_fault가 일어날 때까지 기다리지 않고 바로 내용을 넣어줘야 하므로 필요 없음
			 return false;
		
			if(!page_table_copy(src_page, upage))
				return false;

			// 매핑된 프레임에 내용 로딩
			struct page *dst_page = spt_find_page(dst, upage);
			if(src_page->frame != NULL)
				memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
	}
	  else{
		return false;
	  }


   }
    return true;
}

void page_desturctor(struct hash_elem *e, void * aux){
	struct page *page = hash_entry(e, struct page, page_hashelem);
    if (page->operations->destroy != NULL) {
        vm_dealloc_page(page);
    }
}


/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt)
{
	/*hash 테이블 순회하면서 동시에 엔트리 삭제(hash_delete)하면 안됨
	그럼 내부 구조가 바뀌어버리니 iterator가 안전하게 동작 하지 않음 
	*/
	// hash_destroy(&spt->spt_hash, page_desturctor);
	hash_clear(&spt->main_table, page_desturctor);
}