#include "filesys/filesys.h" // 잡았다 요놈!
#include "userprog/process.h" // 잡았다 요놈!
#include "threads/palloc.h"
#include "filesys/file.h"
#include "vm/vm.h"

#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file) ;
void check_address(const uint64_t *addr);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

#define PAL_ZERO 0


/**
 * 인라인 함수.
 * 해당 주소값이 유저 영역(<0x8004000000)내인지를 검증
 * 
 * @param addr: 주소값.
 */
inline void check_address(const uint64_t *addr){

	if (addr == NULL || !(is_user_vaddr(addr)) ){
		exit(-1);
	}
}

/**
 * 인라인 함수.
 * 해당 범위가 유저 영역(<0x8004000000)내인지를 검증
 * 
 * @param addr: 주소값.
 */
inline void check_address_with_size(const uint64_t *addr, size_t size){
	if (addr == NULL || !is_user_vaddr(addr) || !is_user_vaddr(addr + size - 1))
		exit(-1);
}

// /**
//  * 아래 검증 래퍼 함수들의 본체.
//  * 
//  * @param user_addr 유저 주소
//  * @param size 버퍼의 사이즈
//  * @param writable "버퍼 입장에서" writable 여부
//  */
// static void validate_user_memory(const void *buffer, size_t size, bool writable) {
// 	if (buffer == NULL || !is_user_vaddr(buffer) || !is_user_vaddr(buffer + size - 1))
// 		exit(-1);

// 	struct thread *curr = thread_current();
// 	uint8_t *start = (uint8_t *)pg_round_down(buffer);
// 	uint8_t *end = (uint8_t *)pg_round_down(buffer + size - 1);

// 	for (uint8_t *addr = start; addr <= end; addr += PGSIZE) {
// 		if (!is_user_vaddr(addr))
// 			exit(-1);

// 		void *page = pml4_get_page(curr->pml4, addr);

// 		if (page == NULL) {
// 			// 페이지 폴트 처리
// 			if (!vm_try_handle_fault(NULL, addr, true, writable, true))
// 				exit(-1);
// 		} else if (writable) {
// 			/**
// 			 * 상세 설명
// 			 *
// 			 * 이미 매핑되어 있는 페이지의 경우, 쓰기 권한이 실제로 존재하는지 확인해야 한다.
// 			 * 이는 단순히 pml4에 해당 가상 주소가 존재하는지만 확인하는 것으로는 충분하지 않다.
// 			 * 
// 			 * 가령, 코드 영역처럼 읽기만 가능하고 쓰기 권한이 없는 페이지는 
// 			 * 유효한 매핑을 가지고 있을 수 있지만, 
// 			 * 그 주소에 write하려고 하면 segmentation fault가 발생해야 한다.
// 			 * 
// 			 * 고로, writable == true인 경우에는 해당 PTE를 직접 추적하여
// 			 * pte의 writable bit를 확인한다.
// 			 * 
// 			 *   - pml4e_walk(): 해당 가상 주소에 대한 최종 PTE를 획득.
// 			 *   - is_writable(): 얻은 PTE가 쓰기 가능한지를 확인.
// 			 */
// 			uint64_t *pte = pml4e_walk(curr->pml4, (uint64_t)addr, false);
// 			if (pte == NULL || !is_writable(pte))
// 				exit(-1);
// 		}
// 	}
// }

// /**
//  * 주의! 시스템 콜 중 write() 등에 사용!
//  * 유저가 전달한 주소가 읽기 가능한 메모리 영역인지 확인.
//  * 주어진 주소 범위내 각 페이지가 유저 공간에 속하며 페이지 폴트를 통해 접근이 가능한지 확인.
//  * 접근 불가능한 경우 프로세스를 종료.
//  * 
//  * @param user_addr 유저 주소
//  * @param size 버퍼의 사이즈
//  */
// void validate_read_buffer(const void *user_addr, size_t size) {
// 	validate_user_memory(user_addr, size, false);
// }

// /**
//  * 주의! 시스템 콜 중 read() 등에 사용!
//  * 유저가 전달한 주소가 쓰기 가능한 메모리 영역인지 확인.
//  * 주어진 주소 범위내 각 페이지가 유저 공간에 속하고 페이지 폴트를 통해 접근 가능하며 쓰기도 가능한지 확인.
//  * 하나라도 만족하지 못하는 경우 프로세스를 종료.
//  * 
//  * @param user_addr 유저 주소
//  * @param size 버퍼의 사이즈
//  */
// void validate_write_buffer(const void *user_addr, size_t size) {
// 	validate_user_memory(user_addr, size, true);
// }

// /**
//  * validate_buffer의 설명과 동일.
//  * 
//  * @param user_addr 유저 주소
//  * @param size 버퍼의 사이즈
//  */
// void validate_buffer(const void *user_addr, size_t size) {
// 	// 기존 로직에 따라 읽기용으로 간주
// 	validate_user_memory(user_addr, size, false);
// }



// writeable 관련 오류가 뜰... sudo?
void validate_buffer(const void *buffer, unsigned size)
{
	// 예외 처리
	if (buffer == NULL)
	{
		exit(-1);
	}

	uint8_t *start = (uint8_t *)pg_round_down(buffer);
	uint8_t *end = (uint8_t *)pg_round_down(buffer + size - 1);
	struct thread *cur = thread_current();

	for (uint8_t *addr = start; addr <= end; addr += PGSIZE)
	{
		if (!is_user_vaddr(addr))
		{
			exit(-1);
		}

		if (pml4_get_page(cur->pml4, addr) == NULL)
		{
			if (!vm_try_handle_fault(NULL, addr, true, false, true))
				exit(-1);
		}
	}
}

void validate_read_buffer(const void *buffer, unsigned size)
{
	if (is_user_vaddr(buffer) == false || is_user_vaddr(buffer + size - 1) == false)
	{
		exit(-1);
	}
	validate_buffer(buffer, size);
}

void validate_write_buffer(const void *buffer, unsigned size)
{
	if (is_user_vaddr(buffer) == false || is_user_vaddr(buffer + size - 1) == false)
	{
		exit(-1);
	}
	validate_buffer(buffer, size);

	uint8_t *start = (uint8_t *)pg_round_down(buffer);
	uint8_t *end = (uint8_t *)pg_round_down(buffer + size - 1);
	struct thread *cur = thread_current();

	for (uint8_t *addr = start; addr <= end; addr += PGSIZE)
	{
		if (!is_user_vaddr(addr))
			exit(-1);

		void *page = pml4_get_page(cur->pml4, addr);
		if (page == NULL)
		{
			// 여기서는 writeable=true로 fault 처리 시도
			if (!vm_try_handle_fault(NULL, addr, true, true, true))
				exit(-1);
		}
		else
		{
			// 이미 매핑되어 있다면 write 권한 확인
			uint64_t *pte = pml4e_walk(cur->pml4, (uint64_t)addr, false);
			struct page *cur_page = spt_find_page(&cur->spt, addr);
			bool page_writable = cur_page->writable;
			if (pte == NULL || (!is_writable(pte) && !page_writable))
			{
				exit(-1); // 쓰기 권한이 없으면 종료
			}
			else if (pte == NULL || (!is_writable(pte) && page_writable))
			{
				vm_try_handle_fault(NULL, addr, true, true, true);
			}
		}
	}
}


/**
 * mmap
 */
void* mmap(void *addr, size_t length, int writable, int fd, off_t offset){
	#ifdef VM
	struct thread *curr = thread_current();
	struct file *file;

	// may fail if the file opened as fd has a length of zero bytes
	if ((long long)length <= 0)
		return NULL;

	// must fail if addr is not page-aligned
	if (pg_round_down(addr) != addr)
		return NULL;

	// Tries to mmap an invalid offset
	if (offset > (off_t)length)
		return NULL;

	if (offset % PGSIZE != 0)
		return NULL;

	// must fail if overlaps any existing set of mapped pages
	if (spt_find_page(&curr->spt, addr))
		return NULL;

	// if addr is 0, it must fail
	if (addr == NULL)
		return NULL;
	
	// try to mmap over kernel
	// addr이 커널 시작 주소인지 || 64비트 이상으로 사용하는지
	if ((long long)addr == KERN_BASE || is_kernel_vaddr(addr))
		return NULL;

	// the fd representing console input and output are not mappable.
	if (fd == 0 || fd == 1)
		return NULL;

	// file open as fd
	if (!(file = curr->fd_table[fd]))
		return NULL;

	return do_mmap(addr, length, writable, file, offset);
	#endif
}

void munmap(void *addr){
	#ifdef VM
	do_munmap(addr);
	#endif
}

/**
 * halt - 머신을 halt함.
 * 
 */
void halt(void){
    power_off();
}

/**
 * seek - 파일을 탐색.
 * 
 * @param fd: 파일 디스크립터.
 * @param position: 위치.
 */
void seek(int fd, unsigned position) {
	struct file *file = process_get_file_by_fd(fd);
	if (file == NULL)
		return;
	file_seek(file, position);
}

/**
 * exit - 해당 프로세스를 종료시킴.
 * 
 * @param status: 현재 상태를 가져오는 파라미터.
 */
void exit(int status) {
	struct thread *curr = thread_current();
    curr->exit_status = status; // 프로그램이 정상적으로 종료되었는지 확인(정상적 종료 시 0)

	printf("%s: exit(%d)\n", thread_name(), status); // 디버그용
	thread_exit(); // 스레드 종료
}

/**
 * write - fd에 buffer로부터 size만큼을 작성.
 * 
 * @param fd: dst인 파일 디스크립터.
 * @param buffer: src인 버퍼.
 * @param size: 복사할 사이즈.
 */
int write(int fd, const void *buffer, unsigned size){
    validate_read_buffer(buffer, size); // 이거 validate_read_buffer라고 쓴 거 맞습니다!!!!!

	struct thread *curr = thread_current();

	lock_acquire(&filesys_lock);

	int bytes_write = 0;
	if (fd == STDOUT_FILENO) { // stdout면 직접 작성
		putbuf(buffer, size);
		bytes_write = size;
	} else {
		if (fd < 2) {
			lock_release(&filesys_lock);
			return -1;
		}

		struct file *file = process_get_file_by_fd(fd);
		if (file == NULL){
			lock_release(&filesys_lock);
			return -1;
		}
		bytes_write = file_write(file, buffer, size);
	}
	lock_release(&filesys_lock);

	return bytes_write;
}

/**
 * fork - 새 자식 프로세스 (스레드아님??) 생성.
 * 성공일 경우 fd, 실패일 경우 -1.
 * 
 * @param file_name: 실행 파일명.
 */
tid_t fork(char *thr_name, struct intr_frame *if_) {
	return process_fork(thr_name, if_);
}

/**
 * exec - 현재 프로세스를 file_name의 실행 파일로 변경.
 * 
 * @param file_name: 실행 파일명.
 */
int exec(char *file_name) {
	check_address(file_name);
	
	// 새로운 페이지 한 장(4KB)을 제로필 후 확보.
	// June 02 : User Zero 동시 옵션으로 업데이트, 도움이 되느지는 잘 모르..겟ㅇ어요
	char *page_copied = palloc_get_page(PAL_USER | PAL_ZERO); 
	if (page_copied == NULL){
		palloc_free_page(page_copied);
		return -1;
	}
	
	// 파라미터의 file_name을, 방금 확보한 커널 메모리에 복사.
	strlcpy(page_copied, file_name, strlen(file_name) + 1); // strlcpy는 마지막에 NULL자를 포함함. 그래서 +1.
	//process_exec()는 현재 프로세스의 전체 주소 공간, 코드, 데이터, 스택 등을 새로운 실행 파일(fn_copy)로 로드.
	if (process_exec(page_copied) == -1)
		exit(-1); // exec에 실패 시 즉시 종료.

	NOT_REACHED();
	return 0;
}

/**
 * create - 파일을 생성.
 * 성공일 경우 true, 실패일 경우 false.
 * 
 * @param file: 생성할 파일의 이름 및 경로 정보.
 * @param initial_size: 생성할 파일의 크기.
 */
bool create(const char *file, unsigned initial_size) {		
	check_address(file);

	lock_acquire(&filesys_lock);
	bool result = filesys_create(file, initial_size);
	lock_release(&filesys_lock);

	return result;
}

/**
 * remove - 파일을 삭제.
 * 성공: true, 실패: false.
 *
 * @param file: 삭제할 파일.
 */
bool remove(const char *file) {
	check_address(file);
	bool result;

	lock_acquire(&filesys_lock);
	result = filesys_remove(file);
	lock_release(&filesys_lock);

	return result;
}


/**
 * open - 파일을 오픈.
 * 성공일 경우 fd, 실패일 경우 -1.
 * 
 * @param file: 오픈할 파일.
 */
int open(const char *filename) {
	check_address(filename); // 이상한 포인터면 즉시 종료
	int fd = -1;

	lock_acquire(&filesys_lock);
	struct file *file_obj = filesys_open(filename);
	
	if (file_obj == NULL) {
		goto done;
	}

	fd = process_add_file(file_obj);

	if (fd == -1) { // fd table 꽉찬 경우 그냥 닫아버림
		// file_close(file_obj);
    	file_obj = NULL;
		goto done;
	}
	
	goto done;
done:
	lock_release(&filesys_lock);
	return fd;
}

/**
 * close - 파일을 닫음.
 * 파일 디스크립터를 닫고, 파일 리소스를 해제.
 *
 * @param fd: 닫을 파일 디스크립터.
 */
void close(int fd) {
	struct file *file_obj = process_get_file_by_fd(fd);
	if (file_obj == NULL)
		return;

	lock_acquire(&filesys_lock);
	file_close(file_obj);
	lock_release(&filesys_lock);

	process_close_file_by_id(fd);
}

/**
 * filesize - 파일사이즈를 리턴.
 * 성공일 경우 해당 크기, 실패일 경우 -1.
 * 
 * @param fd: 파일 디스크립터.
 */
int filesize(int fd) {
	struct file *open_file = process_get_file_by_fd(fd);
	if (open_file == NULL) {
		return -1;
	}
	return file_length(open_file);
}

/**
 * read - 파일을 읽음.
 * 성공일 경우 fd, 실패일 경우 -1.
 * 
 * @param fd: 파일 디스크립터.
 * @param buffer: 버퍼.
 * @param size: 복사할 크기.
 */
int read(int fd, void *buffer, unsigned size){
	// 1. 버퍼를 검증
    validate_write_buffer(buffer, size); // 이거 validate_write_buffer라고 쓴 거 맞습니다!!!!!

    if (size == 0)
        return 0;
    // if (buffer == NULL || !is_user_vaddr(buffer))
    //     exit(-1);

    // 2. stdin (fd == 0)일 경우
    if (fd == STDIN_FILENO) {
        unsigned i;
        uint8_t *buf = buffer;
        for (i = 0; i < size; i++) 
            buf[i] = input_getc();
        return i;
    }

    // 3. fd 범위 검사
    if (fd < 2 || fd >= FDCOUNT_LIMIT)
        return -1;

    struct file *file = process_get_file_by_fd(fd);
    if (file == NULL)
        return -1; // 해당 파일이 NULL이면 즉시 리턴.

    // 4. 정상적인 파일이면 read
    off_t ret;
    lock_acquire(&filesys_lock);
    ret = file_read(file, buffer, size);
    lock_release(&filesys_lock);
    return ret;
}

void syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock); // Project 2. User Programs
}

/* The main system call interface */
void syscall_handler (struct intr_frame *f UNUSED) {
	int sys_call_number = (int) f->R.rax; // 시스템 콜 번호 받아옴
	// printf("syscall_handler - sys_call_number: %d\n",sys_call_number);

	// Project 3. Virtual Memory ~
	/**
	 * 시스템 콜 내부(커널 진입)에서 사용자 버퍼에 접근하다가, 
	 * stack 영역에서 page fault가 발생할 수 있다.
	 * 이때, 커널이 현재의 rsp(유저 스택의 포인터)가 어딘지를 알아야 
	 * "정말로 stack 확장이 맞는지"를 판단 가능.
	 */
	thread_current()->rsp = f->rsp; // Gitbook의 Stack Growth 페이지 참조!!!!
	// ~ Project 3. Virtual Memory

	/*
	 x86-64 규약은 함수가 리턴하는 값을 "rax 레지스터"에 담음. 다른 인자들은 rdi, rsi 등 다른 레지스터로 전달.
	 시스템 콜들 중 값 반환이 필요한 것은, struct intr_frame의 rax 멤버 수정을 통해 구현
	 */
	switch (sys_call_number) {		//  system call number가 rax에 있음.
		case SYS_HALT:
			// printf("SYS_HALT [%d]\n", sys_call_number);
			halt();	 // Pintos 자체를 종료
			break;
		case SYS_EXIT:
			// printf("SYS_EXIT [%d]\n", sys_call_number);
			exit(f->R.rdi);	// 현재 프로세스를 종료
			break;
		case SYS_FORK:
			// printf("SYS_FORK [%d]\n", sys_call_number);
			f->R.rax = fork(f->R.rdi, f);
			break;
		case SYS_EXEC:
			// printf("SYS_EXEC [%d]\n", sys_call_number);
			f->R.rax = exec(f->R.rdi);
       		break;
		case SYS_WAIT:
			// printf("SYS_WAIT [%d]\n", sys_call_number);
			f->R.rax = process_wait(f->R.rdi);
			break;
		case SYS_CREATE:
			// printf("SYS_CREATE [%d]\n", sys_call_number);
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			// printf("SYS_REMOVE [%d]\n", sys_call_number);
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:
			// printf("SYS_OPEN [%d]\n", sys_call_number);
    		f->R.rax = open((const char *)f->R.rdi);
			break;
		case SYS_FILESIZE:
			// printf("SYS_FILESIZE [%d]\n", sys_call_number);
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			// printf("SYS_READ [%d]\n", sys_call_number);
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			// printf("SYS_WRITE [%d]\n", sys_call_number);
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			// printf("SYS_SEEK [%d]\n", sys_call_number);
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			// printf("SYS_TELL [%d]\n", sys_call_number);
			// f->R.rax = tell(f->R.rdi);
			// printf("tell() undefined! [%d]", sys_call_number);
			break;
		case SYS_CLOSE:
			// printf("SYS_CLOSE [%d]\n", sys_call_number);
			close(f->R.rdi);
			break;
		case SYS_MMAP:
			// printf("SYS_MMAP [%d]\n", sys_call_number);
			f->R.rax = mmap((void *)f->R.rdi, (size_t)f->R.rsi, (int)f->R.rdx, (int)f->R.r10, (off_t)f->R.r8);
			break;
		case SYS_MUNMAP:
			// printf("SYS_MUNMAP [%d]\n", sys_call_number);
			munmap((void *)f->R.rdi);
			break;
		default:
			printf("FATAL: UNDEFINED SYSTEM CALL!, %d", sys_call_number);
			exit(-1);
			break;
	}
	// thread_exit ();
}
