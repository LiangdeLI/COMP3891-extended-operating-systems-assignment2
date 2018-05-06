/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <mips/trapframe.h>
#include <proc.h>

/*
 * Put your function declarations and data types here ...
 */



struct fd_table {
  struct openFile * fnode;
};

struct openFile {
  int flags;
  off_t offset;
  int refCount;
  struct lock * filelock;
  struct vnode * vNode;
};

struct proc_info {
	struct proc * curr_proc;
	pid_t pid;
	struct proc_info * next;
};

void init_fdesc(void);

int sys_open(const_userptr_t file, int flag, mode_t mode, int32_t* retval);

int sys_close(int handle, int32_t * retval);

int sys_read(int handle, void * buf, size_t len, int32_t * retval);

int sys_write(int handle, void * buf, size_t len, int32_t * retval);

int sys_dup2(int old_handle, int new_handle, int32_t* retval);

int sys_lseek(int fd, off_t pos, int whence, off_t* retval);

int sys_fork(struct trapframe* tf, pid_t * ret_pid);

void sys_getpid(pid_t * ret_pid);

bool havePid(struct proc * test_proc);

pid_t assignPid(struct proc * test_proc);

#endif /* _FILE_H_ */
