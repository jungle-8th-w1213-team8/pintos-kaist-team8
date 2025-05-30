#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "lib/kernel/hash.h"

enum vm_type {
	/* 페이지가 미초기화(uninitialized) 상태. */
	VM_UNINIT = 0,
	/* 페이지가 파일에 관련되지 않음, 즉, 익명 페이지. */
	VM_ANON = 1,
	/* 페이지가 파일에 관련됨. */
	VM_FILE = 2,
	/* 페이지 캐시를 보유한 페이지. Project 4용. */
	VM_PAGE_CACHE = 3,

	/* 상태(state) 저장용 비트 플래그들. */

	/* 정보를 저장하기 위한 보조(auxillary) 비트 플래그 마커입니다. 
	   int 범위 안에서 더 많은 마커를 추가할 수 있습니다. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* 이 값 넘지 마세요. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* "page"를 나타냅니다.
 * 이것은 일종의 "부모 클래스"로, 
 * 네 개의 "자식 클래스(uninit_page, file_page, anon_page, page cache(project 4))"를 가집니다.
 * 이 구조체의 미리 정의된 멤버는 삭제/수정하지 마세요. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */

	/* 타입별 데이터는 union에 바인드됩니다.
	 * 각 함수는 현재의 union을 자동으로 감지합니다. - 이게 무슨 소리야? */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* "frame"을 나타냅니다. */
struct frame {
	void *kva;
	struct page *page;
};

/* 이것은 page operations를 위한 함수 테이블입니다. 
 * C에서 "interface"를 구현하는 한 가지 방법으로,
 * "method"의 테이블을 구조체의 멤버로 넣어,
 * 필요할 때 호출합니다. */
struct page_operations {
	bool (*swap_in) (struct page *, void *); // "struct page_operations 안에 swap_in이라는 함수 포인터 멤버를 둔다"는 의미.
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type; // 이 타입에 의해 struct page의 union이 결정되도록 구성 가능.
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* 현재 프로세스의 메모리 공간(memory space)을 나타냅니다.
 * 저희는 이 구조체에 대해 특정한 설계를 강제하지 않습니다.
 * 모든 설계는 여러분에게 달려 있습니다. */
struct supplemental_page_table {
    struct hash pages;       /* 해시 테이블 */
    struct lock spt_lock;    /* 동시성 제어용 락 */
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

#endif  /* VM_VM_H */
