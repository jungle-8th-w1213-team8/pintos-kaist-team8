#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void seek(int fd, unsigned position);
void exit(int status) ;
void halt(void);
int write(int fd, const void *buffer, unsigned size);
int exec(char *file_name) ;
int open(const char *filename) ;
void close(int fd);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);

/* 전역 변수 ~ */
struct lock g_filesys_lock;
/* ~ 전역 변수 */

#endif /* userprog/syscall.h */
