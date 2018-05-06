#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <stat.h>
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
	KASSERT(curthread->fdesc[1] != NULL);
	curthread->fdesc[2] = kmalloc(sizeof(struct fd_table));
	KASSERT(curthread->fdesc[2] != NULL);

	char c1[] = "con:";
	char c2[] = "con:";

	vfs_open(c1,O_WRONLY,0,&v1); 
	vfs_open(c2,O_WRONLY,0,&v2); 

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
int sys_open(const_userptr_t file, int flag, mode_t mode, int32_t* retval){
//int sys_open(const_userptr_t file, int flag, mode_t mode){
		
	int res;
	int fd = 3;
	int i;
	struct vnode* vNode = NULL;
	char *f = (char*)kmalloc(sizeof(char)*PATH_MAX);
	size_t len;

	//if the file is NULL, there is no such file.
	if(file == NULL){
		return ENOENT;
	}


	if(f == NULL){
		return ENOMEM;	
	}
	

	//Copy the string to kernel space.
	copyinstr(file, f, PATH_MAX, &len);
	

	
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
	ofTable[i].filelock = lock_create("alock");
	//ofTable[i].refcount++;

	//Connect the file descriptor to open_file_node
	curthread->fdesc[fd]->fnode = &ofTable[i];
	*retval = fd;


	return 0;
}

//System close implementation
int sys_close(int handle, int32_t * retval) {
//int sys_close(int handle) {

	if(handle < 0 || handle >= OPEN_MAX || curthread->fdesc[handle] == NULL){
		*retval = -1;
		return EBADF;	
	}

	
	struct openFile* curr_ofn = curthread->fdesc[handle]->fnode;


	if(curr_ofn->refCount == 0 || curr_ofn->vNode == NULL){
		*retval = -1;
		return EBADF;
	}
		
	//free the file descriptor 
	kfree(curthread->fdesc[handle]);
	curthread->fdesc[handle] = NULL;
	
	//Acquire lock
	lock_acquire(curr_ofn->filelock);
	curr_ofn->refCount--;

	if(curr_ofn->refCount == 0) {
		vfs_close(curr_ofn->vNode);
		//Destroy lock
		lock_release(curr_ofn->filelock);
	    lock_destroy(curr_ofn->filelock);
	    *retval = 0;
		return 0;
	}
	
	//Release lock
	lock_release(curr_ofn->filelock);


	*retval = 0;
	return 0;
}

//System read implementation
int sys_read(int handle, void * buf, size_t len, int32_t * retval) {
//int sys_read(int handle, void * buf, size_t len) {

	if(handle < 0 || handle >= OPEN_MAX || curthread->fdesc[handle] == NULL){
		*retval = -1;
		return EBADF;	
	}
	
	if(!(curthread->fdesc[handle]->fnode->flags != O_RDONLY || curthread->fdesc[handle]->fnode	->flags != O_RDWR)){
		*retval = -1;
		return EINVAL;	
	}




	int res;
	struct uio u;
	struct iovec iov;
	

	//Get the open file node
	struct openFile* curr_ofn = curthread->fdesc[handle]->fnode;


	//Configuration

	lock_acquire(curthread->fdesc[handle]->fnode->filelock);



	uio_kinit(&iov, &u, (void*)buf, len, curr_ofn->offset, UIO_READ);



	//Called the VOP_READ function
	//if(res = VOP_READ(curr_ofn->vNode, &u)) {
	res = VOP_READ(curr_ofn->vNode, &u);

	if(res) {
		//kfree(kbuf);
		lock_release(curthread->fdesc[handle]->fnode->filelock);
		*retval = -1;
		return res;
	}else{
		curr_ofn->offset = u.uio_offset;
		lock_release(curthread->fdesc[handle]->fnode->filelock);
		*retval = len - u.uio_resid;
		//kfree(kbuf);
		return 0;

	}
}

//System write implementation
int sys_write(int handle, void * buf, size_t len, int32_t * retval) {
//int sys_write(int handle, void * buf, size_t len) {

	if(handle < 0 || handle >= OPEN_MAX || curthread->fdesc[handle] == NULL){
		*retval = -1;
		return EBADF;	
	}
	
	if(!(curthread->fdesc[handle]->fnode->flags != O_WRONLY || curthread->fdesc[handle]->fnode->flags != O_RDWR)){
		*retval = -1;
		return EINVAL;	
	}



	int res;
	struct uio u;
	struct iovec iov;

	//Get the open file node
	struct openFile* curr_ofn = curthread->fdesc[handle]->fnode;

	
	//
	void* kbuf = kmalloc(sizeof(*buf) * len);
	if(kbuf == NULL) return EFAULT;
	size_t got;
	copyinstr((const_userptr_t) buf, (char*)kbuf, len, &got);
	//
	lock_acquire(curthread->fdesc[handle]->fnode->filelock);	
    uio_kinit(&iov, &u, (void*)buf, len, curr_ofn->offset, UIO_WRITE);

	
	//if(res = VOP_WRITE(curr_ofn->vNode, &u)) {
	res = VOP_WRITE(curr_ofn->vNode, &u);
	// if(handle!=1 && handle!=2) kprintf("here3\n");
	if(res) {
		kfree(kbuf);
		lock_release(curthread->fdesc[handle]->fnode->filelock);
		* retval = -1;
		return res;
	}else{
		curr_ofn->offset = u.uio_offset;
		lock_release(curthread->fdesc[handle]->fnode->filelock);
		*retval = len - u.uio_resid;
		kfree(kbuf);
		return 0;
		
	}
}


//system duplicate2 implementation
int sys_dup2(int old_handle, int new_handle, int32_t* retval)
{
	struct openFile* old_ofn;
	struct openFile* new_ofn;
	if(curthread->fdesc[old_handle] == NULL){
		return EBADF;	
	}
	
	if((old_handle < 0 || old_handle >= OPEN_MAX) || (new_handle < 0 || new_handle >= OPEN_MAX)){
		*retval = -1;
		return EBADF;
	}

	if(old_handle == new_handle){
		*retval = old_handle;
		return 0;
	}

	//Not sure for what we should if it is not null
	if(curthread->fdesc[new_handle] != NULL){
		//Acquire lock
		new_ofn = curthread->fdesc[new_handle]->fnode;
		lock_acquire(new_ofn->filelock);
		new_ofn->refCount--;
		//Release lock
		lock_release(new_ofn->filelock);
		if(new_ofn->refCount == 0) {
			vfs_close(new_ofn->vNode);
			//destroy lock
		    lock_destroy(new_ofn->filelock);
		}
	}
	
	old_ofn = curthread->fdesc[old_handle]->fnode;
	curthread->fdesc[new_handle]->fnode = old_ofn;
	
	lock_acquire(curthread->fdesc[old_handle]->fnode->filelock);
	old_ofn->refCount++;
	lock_release(curthread->fdesc[old_handle]->fnode->filelock);

	*retval = new_handle;
	return 0;

}

int sys_lseek(int fd, off_t pos, int whence, off_t* retval)
{
	//fd is not a valid file handle.
	if(curthread->fdesc[fd] == NULL || fd < 0 || fd >= OPEN_MAX)
	{
		*retval = -1;
		return EBADF;
	}

	//fd refers to an object which does not support seeking
	if(!VOP_ISSEEKABLE(curthread->fdesc[fd]->fnode->vNode))
	{
		*retval = -1;
		return ESPIPE;
	}

	//whence is invalid.
	if(whence!=SEEK_SET && whence!=SEEK_CUR && whence!=SEEK_END)
	{
		*retval = -1;
		return EINVAL;
	}
	
	struct stat * stat_ptr = kmalloc(sizeof(struct stat));
	VOP_STAT(curthread->fdesc[fd]->fnode->vNode, stat_ptr);

	//Acqurie lock
	lock_acquire(curthread->fdesc[fd]->fnode->filelock);
	

	if(whence==SEEK_SET)
	{
		//The resulting seek position would be negative.
		if(pos<0) return EINVAL;
		curthread->fdesc[fd]->fnode->offset = pos;
	}
	else if (whence==SEEK_CUR)
	{
		// new offset
		pos += curthread->fdesc[fd]->fnode->offset;
		//The resulting seek position would be negative.
		if(pos<0) return EINVAL;
		curthread->fdesc[fd]->fnode->offset = pos;
	}
	else if (whence==SEEK_END)
	{
		// new offset
		pos += stat_ptr->st_size;
		if(pos<0) return EINVAL;
		curthread->fdesc[fd]->fnode->offset = pos;		
	}
	kfree(stat_ptr);
	//Assign return value
	*retval = curthread->fdesc[fd]->fnode->offset;
	lock_release(curthread->fdesc[fd]->fnode->filelock);

	return 0;
}


//****************************
