/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "lib/kernel/bitmap.h" 
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
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

/* 스왑 슬롯 비트맵 */
struct bitmap *swap_table;
/* 한 페이지를 몇 개의 섹터로 나누어 저장할지 계산 */
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;

/* Initialize the data for anonymous pages */
void vm_anon_init(void) {
    /* 스왑 디스크를 설정 */
    swap_disk = disk_get(1, 1);
    if (!swap_disk) {
        PANIC("No swap disk found!");
    }

    /* swap_table 비트맵 생성 (스왑 슬롯 수: 디스크 크기 / 한 페이지가 차지하는 섹터 수) */
    size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
    swap_table = bitmap_create(swap_size);
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {
    /* 핸들러 설정 */
    page->operations = &anon_ops;

    /* 페이지의 anon_page 구조체 초기화 */
    struct anon_page *anon_page = &page->anon;
    anon_page->swap_idx = -1;

    /* kva가 유효하면 0으로 초기화 */
    if (kva != NULL) {
        memset(kva, 0, PGSIZE);
    }

    return true;
}

/* Swap in the page by reading contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) {
    struct anon_page *anon_page = &page->anon;
    int idx = anon_page->swap_idx;

    /* swap_idx가 유효하지 않으면 실패 */
    if (idx < 0) {
        return false;
    }

    /* swap 디스크에서 512바이트씩 8번 읽어서 kva에 복사 */
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
        disk_read(swap_disk, idx * SECTORS_PER_PAGE + i, kva + DISK_SECTOR_SIZE * i);
    }

    /* 비트맵에서 해당 슬롯 비우기 (false로) */
    bitmap_reset(swap_table, idx);

    /* swap_idx 초기화 */
    anon_page->swap_idx = -1;

    return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) {
    struct anon_page *anon_page = &page->anon;

    /* 비어있는 슬롯을 찾고, 사용 중으로 표시 (race condition 방지) */
    size_t idx = bitmap_scan_and_flip(swap_table, 0, 1, false);

    if (idx == BITMAP_ERROR) {
        PANIC("Swap disk is full!");
        return false;
    }

    /* 디스크에 512바이트씩 8번 기록 (kva → 디스크) */
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
        disk_write(swap_disk, idx * SECTORS_PER_PAGE + i, page->frame->kva + DISK_SECTOR_SIZE * i);
    }

    /* 페이지 ↔ 프레임 연결 끊기 */
    page->frame->page = NULL;
    page->frame = NULL;

    /* 스왑 슬롯 번호 저장 */
    anon_page->swap_idx = idx;

    /* 페이지 테이블에서 이 페이지의 매핑 제거 (다음 접근 시 page fault 발생) */
    pml4_clear_page(thread_current()->pml4, page->va);

    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
    struct anon_page *anon_page = &page->anon;

    /* 스왑 슬롯이 사용 중이면 비트맵에서 false로 되돌리기 */
    if (anon_page->swap_idx >= 0) {
        bitmap_reset(swap_table, anon_page->swap_idx);
    }

    /* 페이지 테이블에서 매핑 제거 */
    pml4_clear_page(thread_current()->pml4, page->va);
}
