#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define PROCESS_MAX_BUF = (1<<7)

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_add_file(struct file *file_obj);
struct file *process_get_file_by_fd(int fd);
struct thread *process_get_child(int pid);
void process_close_file_by_id(int fd);
void argument_stack(char **argv, int argc, void **rsp) ;
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);


bool lazy_load_segment(struct page *page, void *aux);

#endif /* userprog/process.h */
