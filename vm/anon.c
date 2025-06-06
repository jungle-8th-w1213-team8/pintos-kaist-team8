/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
#define SECTORS_PER_PAGE (PGSIZE / DISK_SECTOR_SIZE)  // 4096 / 512 = 8

#include "vm/vm.h"
#include "devices/disk.h"
#include "kernel/bitmap.h"
#include "threads/vaddr.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
struct bitmap *swap_table;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	if(!swap_disk) return;

	size_t total_sectors = disk_size(swap_disk);
	size_t swap_slots = total_sectors / 8;

	swap_table = bitmap_create(swap_slots);
	bitmap_set_all(swap_table, false);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	struct anon_page *anon_page = &page->anon;
	page->operations = &anon_ops;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	//pml4_set_accessed(page->owner->pml4, page->va, true);
	for (int i = 0; i < 8; i++) {
        disk_read(swap_disk, anon_page->swap_num * 8 + i,
                  kva + i * DISK_SECTOR_SIZE);
    }
	bitmap_set(swap_table, anon_page->swap_num, false);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	int swap_slot = bitmap_scan(swap_table, 0, 1, false);
	if (swap_slot == BITMAP_ERROR) return false;

	anon_page->swap_num = swap_slot;
	
	for (int i = 0; i < 8; i++) {
        disk_write(swap_disk, page->anon.swap_num * 8 + i,
                   page->frame->kva + i * DISK_SECTOR_SIZE);
    }
	bitmap_set(swap_table, anon_page->swap_num, true);
	return true;
	
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
    uint64_t *pml4 = thread_current()->pml4;
	struct anon_page *anon_page = &page->anon;
    pml4_clear_page(pml4, page->va);
}
