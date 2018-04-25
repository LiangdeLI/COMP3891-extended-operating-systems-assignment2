#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */



//Add************************** The below code part still not work yet...
//int open(const char *filename, int flags, ...);


// Initialize the file descriptor table
void init_fd(void){


}

// Initialize the open file table
void init_of(void){
	for(int i = 0;i < OPEN_MAX; i++){
		ofTable[i].vNode = NULL;
		ofTable[i].offset = 0;
		ofTable[i].flags = 0;
		ofTable[i].refCount = 0;
		ofTable[i].filelock = lock_creat("of_table_lock");	
	}
	return;
}

int sys_open(const_userptr_t file, int flag, mode_t mode, int32_t retVal){
	
	int fd = 3;
	int res;
	//if the file is NULL, there is no such file.
	if(file == NULL){
		return ENOENT;
	}

	curthread->t_fdtable[fd] = kmalloc(sizeof(struct fdesc));
	
	//if the file decriptor table is null, it means: "Bad memory reference"?
	if(curthread->t_fdtable[fd] == NULL){
		return EFAULT;
	}
	
	struct vnode* vNode = NULL;
	
	// try to call vfs_open to open the file
	if (res = vfs_open((char*) file, flag, mode, &v)){
		return res
	}

	return 0;
}



//****************************
