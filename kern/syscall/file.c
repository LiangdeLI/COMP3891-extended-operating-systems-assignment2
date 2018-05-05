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
	struct vnode *v2 = NULL;

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

	curthread->fdesc[1]->fnode = kmalloc(sizeof(struct openFile));
 	KASSERT(curthread->fdesc[1]->fnode != NULL);
 	curthread->fdesc[2]->fnode = kmalloc(sizeof(struct openFile));
 	KASSERT(curthread->fdesc[2]->fnode != NULL);

 	curthread->fdesc[1]->fnode->vNode = v1;
	curthread->fdesc[1]->fnode->offset = 0;
 	curthread->fdesc[1]->fnode->flags = O_WRONLY;
 	curthread->fdesc[1]->fnode->refCount = 1;
 	curthread->fdesc[1]->fnode->filelock = lock_create("stdout_lock");

 	curthread->fdesc[2]->fnode->vNode = v2;
	curthread->fdesc[2]->fnode->offset = 0;
 	curthread->fdesc[2]->fnode->flags = O_WRONLY;
 	curthread->fdesc[2]->fnode->refCount = 1;
 	curthread->fdesc[2]->fnode->filelock = lock_create("stderr_lock");

 	return;


}
/*
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
*/

//System open implementation
//int sys_open(const_userptr_t file, int flag, mode_t mode, int32_t* retVal){
int sys_open(const_userptr_t file, int flag, mode_t mode){
		
	int res;
	int fd = 3;
	int i;
	struct vnode* vNode = NULL;


	//if the file is NULL, there is no such file.
	if(file == NULL){
		return ENOENT;
	}
	
	//Find available place in file descriptor table		
	for(fd = 3; curthread->fdesc[fd] != NULL; fd++){
		
		if(fd >= OPEN_MAX){
			return ENFILE;		
		}
	}

	curthread->fdesc[fd] = kmalloc(sizeof(struct fd_table));

	//if the file decriptor table is null, it means: "Bad memory reference"?
	if(curthread->fdesc[fd] == NULL){
		return EFAULT;
	}

	
	//curthread->t_fdesc[fd] = kmalloc(sizeof(struct fd_table));

	
	//Find available place in open file table
	for(i=0;ofTable[i].vNode != NULL;i++){
		if(i >= OPEN_MAX) {
			return ENFILE;
		}
	}


	

	// try to call vfs_open to open the file
	//if (res = vfs_open((char*) file, flag, mode, &vNode)){
	res = vfs_open((char*) file, flag, mode, &vNode);
	if (res){
		return res;
	}

	//Update the information in the open file node
	ofTable[i].offset = 0;
	ofTable[i].refCount++;	
	ofTable[i].flags = flag;
	ofTable[i].vNode = vNode;
	//ofTable[i].refcount++;

	//Connect the file descriptor to open_file_node
	curthread->fdesc[fd]->fnode = &ofTable[i];
	//*retVal = fd;


	return 0;
}

//System close implementation
//int sys_close(int handle, int32_t * retVal) {
int sys_close(int handle) {
	
	struct openFile* curr_ofn = curthread->fdesc[handle]->fnode;


	if(curr_ofn->refCount == 0 || curr_ofn->vNode == NULL){
		return EBADF;
	}
		
	//free the file descriptor 
	kfree(curthread->fdesc[handle]);
	curthread->fdesc[handle] = NULL;
	
	//Acquire lock
	lock_acquire(ofTable[handle].filelock);
	curr_ofn->refCount--;

	if(curr_ofn->refCount == 0) {
		curr_ofn->flags = 0;
		curr_ofn->offset = 0;
		vfs_close(curr_ofn->vNode);
		curr_ofn->vNode = NULL;
	}
	
	//Release lock
	lock_release(ofTable[handle].filelock);

	//*retVal = 0;
	return 0;
}

//System read implementation
//int sys_read(int handle, void * buf, size_t len, int32_t * retVal) {
int sys_read(int handle, void * buf, size_t len) {

	int res;
	struct uio u;
	struct iovec iov;
	

	//Get the open file node
	struct openFile* curr_ofn = curthread->fdesc[handle]->fnode;


	//Configuration
	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = len;

	lock_acquire(ofTable[handle].filelock);
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = curr_ofn->offset;
	u.uio_resid = len;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curthread->t_addrspace;


	//Called the VOP_READ function
	//if(res = VOP_READ(curr_ofn->vNode, &u)) {
	res = VOP_READ(curr_ofn->vNode, &u);

	if(res) {
		//kfree(kbuf);
		lock_release(ofTable[handle].filelock);
		//*retVal = -1;
		return res;
	}else{
		curr_ofn->offset = u.uio_offset;
		lock_release(ofTable[handle].filelock);
		//*retVal = len - u.uio_resid;
		//kfree(kbuf);
		return 0;

	}
}

//System write implementation
//int sys_write(int handle, void * buf, size_t len, int32_t * retVal) {
int sys_write(int handle, void * buf, size_t len) {

	if(handle < 0 || handle >= OPEN_MAX || curthread->fdesc[handle] == NULL){
		return ENFILE;	
	}


	int res;
	struct uio u;
	struct iovec iov;

	//Get the open file node
	struct openFile* curr_ofn = curthread->fdesc[handle]->fnode;

	//
	void* kbuf = kmalloc(sizeof(*buf) * len);
	if(kbuf == NULL) return EFAULT;
	copyin((const_userptr_t) buf, kbuf, len);
	//


	//Configuration
	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = len;

	lock_acquire(ofTable[handle].filelock);
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = curr_ofn->offset;
	u.uio_resid = len;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curthread->t_addrspace;



	
	//if(res = VOP_WRITE(curr_ofn->vNode, &u)) {
	res = VOP_WRITE(curr_ofn->vNode, &u);
	if(res) {
		kfree(kbuf);
		lock_release(ofTable[handle].filelock);
		//* retVal = -1;
		return res;
	}else{
		curr_ofn->offset = u.uio_offset;
		lock_release(ofTable[handle].filelock);
		//*retVal = len - u.uio_resid;
		kfree(kbuf);
		return 0;
		
	}
}





//****************************
