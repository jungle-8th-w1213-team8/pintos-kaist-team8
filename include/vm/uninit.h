#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* 초기화되지 않은 페이지. 
   "Lazy loading"을 구현하기 위한 타입입니다. */
struct uninit_page {
	/* 페이지의 콘텐츠를 초기화. */
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	/* struct page 초기화, VA를 PA에 매핑. */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
