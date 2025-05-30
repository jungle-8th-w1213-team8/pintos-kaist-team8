/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */
/* uninit.c: 미초기화(uninitialized) 페이지의 구현.
 *
 * 모든 페이지는 uninit 페이지로 생성된다. 
 * 첫 번째 페이지 폴트가 발생하면,
 * 핸들러 체인은 uninit_initialize(page->operations.swap_in)를 호출한다.
 * uninit_initialize 함수는 페이지 객체를 초기화하여 
 * 해당 페이지를 특정 페이지의 객체(anon, file, page_cache)로 변환(transmute)하고,
 * vm_alloc_page_with_initializer 함수에서 전달된 초기화 콜백을 호출한다. */
#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* 최초 폴트 발생 시 페이지를 초기화 */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* uninit_page가 보유(hold)했던 리소스를 해제. 
 * 대부분의 페이지는 다른 페이지 객체로 변환되지만,
 * 프로세스가 종료될 때, 실행 당시 한 번도 참조되지 않았던 uninit 페이지가 남았을 수 있으므로,
 * PAGE는 호출자가 해제함. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
}
