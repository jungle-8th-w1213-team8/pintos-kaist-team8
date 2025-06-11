// /* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

// #include "vm/vm.h"
// #include "devices/disk.h"
// #include "threads/vaddr.h"
// #include "threads/thread.h"
// #include "lib/kernel/bitmap.h" 
// /* DO NOT MODIFY BELOW LINE */

// static struct disk *swap_disk;
// /* 스왑 슬롯 비트맵 */
// struct bitmap *swap_table;
// /* 한 페이지를 몇 개의 섹터로 나누어 저장할지 계산 */
// const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;
// static bool anon_swap_in(struct page *page, void *kva);
// static bool anon_swap_out(struct page *page);
// static void anon_destroy(struct page *page);

// /* DO NOT MODIFY this struct */
// static const struct page_operations anon_ops = {
//     .swap_in = anon_swap_in,
//     .swap_out = anon_swap_out,
//     .destroy = anon_destroy,
//     .type = VM_ANON,
// };


// /* Initialize the data for anonymous pages */
// void vm_anon_init(void) {
//     swap_disk = disk_get(1, 1);
//     size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
//     swap_table = bitmap_create(swap_size);
// }

// /* Initialize the file mapping */
// bool anon_initializer(struct page *page, enum vm_type type, void *kva) {

//     if(page == NULL) return false;

//     page->operations = &anon_ops;
//     struct anon_page *anon_page = &page->anon;
//     anon_page->swap_idx = -1;

//     return true;
// }

// /* Swap in the page by reading contents from the swap disk. */
// static bool anon_swap_in(struct page *page, void *kva) {
//     struct anon_page *anon_page = &page->anon;
//     int idx = anon_page->swap_idx;

//     /* swap_idx가 유효하지 않으면 실패 */
//     if (idx < 0) return false;

//     /* swap 디스크에서 512바이트씩 8번 읽어서 kva에 복사 */
//     for (int i = 0; i < SECTORS_PER_PAGE; i++) {
//         disk_read(swap_disk, idx * SECTORS_PER_PAGE + i, kva + DISK_SECTOR_SIZE * i);
//     }

//     bitmap_reset(swap_table, idx);
//     anon_page->swap_idx = -1;
//     return true;
// }

// /* Swap out the page by writing contents to the swap disk. */
// static bool anon_swap_out(struct page *page) {
//     if(page==NULL) return false;
//     struct anon_page *anon_page = &page->anon;
//     struct frame *frame = page->frame;

//     /* 비어있는 슬롯을 찾고, 사용 중으로 표시 (race condition 방지) */
//     size_t idx = bitmap_scan_and_flip(swap_table, 0, 1, false);

//     if (idx == BITMAP_ERROR) PANIC("Swap disk is full!");

//     /* 디스크에 512바이트씩 8번 기록 (kva → 디스크) */
//     for (int i = 0; i < SECTORS_PER_PAGE; i++) {
//         disk_write(swap_disk, idx * SECTORS_PER_PAGE + i, frame->kva + DISK_SECTOR_SIZE * i);
//     }

//     frame->r_cnt--;
//     page->frame->page = NULL;
//     page->frame = NULL;

//     anon_page->swap_idx = idx;
//     return true;
// }

// /* Destroy the anonymous page. PAGE will be freed by the caller. */
// static void anon_destroy(struct page *page) {
//     struct anon_page *anon_page = &page->anon;

//     pml4_clear_page(thread_current()->pml4, page->va);


//     /* 스왑 슬롯이 사용 중이면 비트맵에서 false로 되돌리기 */
//     if (anon_page->swap_idx != -1) {
//         bitmap_reset(swap_table, anon_page->swap_idx);
//     }

//     if (page->frame != NULL) {
// 		page->frame->r_cnt--;
// 		if(page->frame->r_cnt==0){
// 			list_remove(&page->frame->f_elem);
// 			palloc_free_page(page->frame->kva);
// 			page->frame->page = NULL; // 연결 해제 (구현에 따라)
// 			free(page->frame);
// 		}
//     }

//     /* 페이지 테이블에서 매핑 제거 */
// }

/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"
#include "devices/disk.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
struct bitmap *swap_table;

static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);


/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	/** TODO: bitmap 자료구조로 스왑 테이블 만들기
	 * bitmap_create로 만들기
	 * 스왑 테이블 엔트리에 이 엔트리가 비어있다는 비트 필요
	 * bitmap 공부가 필요할듯
	 */
	swap_table = bitmap_create(disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE));
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	if(page==NULL){
		return false;
	}
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_idx = -1;
	

	return true;
}

/* 스왑 디스크에서 내용을 읽어와 페이지를 스왑인합니다. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	/** TODO: 페이지 스왑 인
	 * disk_read를 데이터를 읽고 kva에 데이터 복사
	 * swap_idx를 -1로 바꿔주어야 함
	 * 프레임 테이블에 해당 프레임 넣어주기
	 * 프레임하고 페이지 매핑해주기
	 */

	struct anon_page *anon_page = &page->anon;
	int swap_idx = anon_page->swap_idx;
	if(swap_idx !=-1){
		for(int i=0; i<8; i++){
			disk_read(swap_disk, (swap_idx * 8 )+ i , kva + (i * DISK_SECTOR_SIZE));
		}
		
		bitmap_set(swap_table, swap_idx, false);
		anon_page->swap_idx = -1;
		return true;
	}
	return false;

}

/* 페이지의 내용을 스왑 디스크에 기록하여 스왑아웃합니다. */
static bool
anon_swap_out(struct page *page)
{
	
	/** TODO: disk_write를 사용하여 disk에 기록
	 * 섹터 크기는 512바이트라 8번 반복해야합니다
	 * 비어있는 스왑 슬롯을 스왑 테이블에서 검색
	 * 검색된 스왑 슬롯 인덱스를 anon_page에 저장
	 * disk_write를 통해 해당 디스크 섹터에 저장
	 */
	if(page==NULL){
		return false;
	}
	struct anon_page *anon_page = &page->anon;
	struct frame *frame = page->frame;

	int table_idx = bitmap_scan_and_flip(swap_table, 0, 1, 0);
	if (table_idx == BITMAP_ERROR)
	{
		ASSERT(bitmap_test(swap_table, table_idx) == false);
		return false;
	}


	for(int i=0; i<8; i++){
		disk_write(swap_disk, (table_idx * 8) + i, frame->kva + (DISK_SECTOR_SIZE * i));
	}

	frame->r_cnt--;
	page->frame->page = NULL;
	page->frame = NULL;

	anon_page->swap_idx=table_idx;

	return true;

}

/* 익명 페이지를 소멸시킵니다. PAGE는 호출자가 해제합니다. */
/** page->frame이 존재할 경우:
 *프레임 테이블에서 제거 (list_remove)
 *물리 메모리 해제 (palloc_free_page)
 *프레임 구조체 메모리 해제 (free)
 */
static void
anon_destroy(struct page *page)
{
    struct anon_page *anon_page = &page->anon;

    pml4_clear_page(thread_current()->pml4, page->va);

    if (anon_page->swap_idx != -1)
        bitmap_set(swap_table, anon_page->swap_idx, false);

    if (page->frame != NULL) {
		page->frame->r_cnt--;
		if(page->frame->r_cnt==0){
			list_remove(&page->frame->f_elem);
			palloc_free_page(page->frame->kva);
			page->frame->page = NULL; // 연결 해제 (구현에 따라)
			free(page->frame);
		}
    }

	
}
