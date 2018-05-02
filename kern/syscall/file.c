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
void init_fdesc(void){
	struct vnode *v1 = NULL;
	stryct *v2 = NULL;

	curthread->fdesc[1] = kmalloc(sizeof(struct fd_table));
	curthread->fdesc[2] = kmalloc(sizeof(struct fd_table));

	char c1[] = "con:";
	char c2[] = "con:";

	vfs_open(c1,O_RDONLY,0,&v1); 
	vfs_open(c2,O_RDONLY,0,&v2); 

	//con = kstrdup("con:");
	//vfs_open(con, O_RDONLY, 0, &vnout);
	//con = kstrdup("con:");
	//vfs_open(con, O_RDONLY, 0, &vnout);

	curthread->fdesc[1]->ofnode = kmalloc(sizeof(struct openFile));
 	KASSERT(curthread->fdesc[1]->ofnode != NULL);
 	curthread->fdesc[2]->ofnode = kmalloc(sizeof(struct openFile));
 	KASSERT(curthread->fdesc[2]->ofnode != NULL);

 	curthread->fdesc[1]->ofnode->vNode = v1;
	curthread->fdesc[1]->ofnode->offset = 0;
 	curthread->fdesc[1]->ofnode->flags = O_WRONLY;
 	curthread->fdesc[1]->ofnode->refCount = 1;
 	curthread->fdesc[1]->ofnode->filelock = lock_create("stdout_lock");

 	curthread->fdesc[2]->ofnode->vNode = v2;
	curthread->fdesc[2]->ofnode->offset = 0;
 	curthread->fdesc[2]->ofnode->flags = O_WRONLY;
 	curthread->fdesc[2]->ofnode->refCount = 1;
 	curthread->fdesc[2]->ofnode->filelock = lock_create("stderr_lock");

 	return;


}

// Initialize the open file table (Although I am not sure we need open file table but just leave here first)
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

//System open implementation
int sys_open(const_userptr_t file, int flag, mode_t mode, int32_t retVal){
		
	int res;
	int fd = 3;
	//if the file is NULL, there is no such file.
	if(file == NULL){
		return ENOENT;
	}
	
	//Find available place in file descriptor table		
	while(curthread->fdesc[fd] != NULL){
		fd++;
		if(fd >= OPENMAX){
			return ENFILE;		
		}
	}

	curthread->fdesc[fd] = kmalloc(sizeof(struct fd_table));

	//if the file decriptor table is null, it means: "Bad memory reference"?
	if(curthread->fdesc[fd] == NULL){
		return EFAULT;
	}

	
	//curthread->t_fdesc[fd] = kmalloc(sizeof(struct fd_table));

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
