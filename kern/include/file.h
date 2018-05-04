/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */

struct of ofTable[OPEN_MAX];

struct fd_table {
  struct of * fnode;
};

struct openFile {
  //char * filename;
  int flags;
  off_t offset;
  int refCount;
  struct lock * filelock;
  struct vnode * vNode;
};

void init_fdesc(void);
void init_of(void);
int sys_open(const_userptr_t filename, int flags, mode_t mode, int32_t * retval);
int sys_close(int handle, int32_t * retval);
int sys_read(int handle, void * buf, size_t len, int32_t * retval);
int sys_write(int handle, void * buf, size_t len, int32_t * retval);



#endif /* _FILE_H_ */
