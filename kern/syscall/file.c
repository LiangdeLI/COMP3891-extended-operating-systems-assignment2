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
#include <addrspace.h>

/*
 * Add your file-related functions here ...
 */



//Add************************** 

// global OPT
struct openFile ofTable[OPEN_MAX];

// helper function for fork
void enter_forked_process(void *tf, unsigned long addr);

// Initialize the file descriptor table
void init_fdesc(void){
	struct vnode *v1 = NULL;
	struct vnode *v2 = NULL;

	curproc->fdesc[1] = kmalloc(sizeof(struct fd_table));
	KASSERT(curproc->fdesc[1] != NULL);
	curproc->fdesc[2] = kmalloc(sizeof(struct fd_table));
	KASSERT(curproc->fdesc[2] != NULL);

	char c1[] = "con:";
	char c2[] = "con:";

	vfs_open(c1,O_WRONLY,0,&v1); 
	vfs_open(c2,O_WRONLY,0,&v2); 

	curproc->fdesc[1]->fnode = kmalloc(sizeof(struct openFile));
 	KASSERT(curproc->fdesc[1]->fnode != NULL);
 	curproc->fdesc[2]->fnode = kmalloc(sizeof(struct openFile));
 	KASSERT(curproc->fdesc[2]->fnode != NULL);

 	// set struct openFile for stdout
 	curproc->fdesc[1]->fnode->vNode = v1;
	curproc->fdesc[1]->fnode->offset = 0;
 	curproc->fdesc[1]->fnode->flags = O_WRONLY;
 	curproc->fdesc[1]->fnode->refCount = 1;
 	curproc->fdesc[1]->fnode->filelock = lock_create("stdout_lock");

 	// set struct openFile for stderr
 	curproc->fdesc[2]->fnode->vNode = v2;
	curproc->fdesc[2]->fnode->offset = 0;
 	curproc->fdesc[2]->fnode->flags = O_WRONLY;
 	curproc->fdesc[2]->fnode->refCount = 1;
 	curproc->fdesc[2]->fnode->filelock = lock_create("stderr_lock");

 	return;

}

//System open implementation
int sys_open(const_userptr_t file, int flag, mode_t mode, int32_t* retval)
{
	int res;
	int fd = 3;
	int i;
	struct vnode* vNode = NULL;
	// kmalloc a buffer in kernel space
	char *f = (char*)kmalloc(sizeof(char)*PATH_MAX);
	KASSERT(f != NULL);
	
	size_t len;

	//if the file is NULL, there is no such file.
	if(file == NULL){
		return ENOENT;
	}

	if(f == NULL){
		return ENOMEM;	
	}	

	//Check and copy the user-level string to kernel space.
	copyinstr(file, f, PATH_MAX, &len);
	
	//Find available place in file descriptor table		
	for(fd = 3; curproc->fdesc[fd] != NULL; fd++){	
		if(fd >= OPEN_MAX){
			return ENFILE;		
		}
	}

	curproc->fdesc[fd] = kmalloc(sizeof(struct fd_table));
	KASSERT(curproc->fdesc[fd] != NULL);

	//if the file decriptor table is null, it means: "Bad memory reference"?
	if(curproc->fdesc[fd] == NULL){
		return EFAULT;
	}

	//Find available place in open file table
	for(i=0;ofTable[i].vNode != NULL;i++){
		if(i >= OPEN_MAX) {
			return ENFILE;
		}
	}

	// try to call vfs_open to open the file
	res = vfs_open((char*) f, flag, mode, &vNode);
	if (res){
		kfree(f);
		return res;
	}

	//Update the information in the open file node
	ofTable[i].offset = 0;
	ofTable[i].refCount=1;	
	ofTable[i].flags = flag;
	ofTable[i].vNode = vNode;
	ofTable[i].filelock = lock_create("alock");

	//Connect the file descriptor to open_file_node
	curproc->fdesc[fd]->fnode = &ofTable[i];
	
	// free kernel space buffer
	kfree(f);
	
	//set return value of this syscall
	*retval = fd;

	return 0;
}

//System close implementation
int sys_close(int handle, int32_t * retval) {
	// fd is not a valid file handle.
	if(handle < 0 || handle >= OPEN_MAX || curproc->fdesc[handle] == NULL){
		*retval = -1;
		return EBADF;	
	}
	
	struct openFile* curr_ofn = curproc->fdesc[handle]->fnode;

	if(curr_ofn->refCount == 0 || curr_ofn->vNode == NULL){
		*retval = -1;
		return EBADF;
	}
		
	//free the file descriptor 
	kfree(curproc->fdesc[handle]);
	curproc->fdesc[handle] = NULL;
	
	//Acquire lock
	lock_acquire(curr_ofn->filelock);
	
	// decrement the reference count of this openFile
	curr_ofn->refCount--;

	// if no more fd refer to this openFile
	if(curr_ofn->refCount == 0) {
		vfs_close(curr_ofn->vNode);
		//Release and Destroy lock
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
	//fd is not a valid file descriptor
	if(handle < 0 || handle >= OPEN_MAX || curproc->fdesc[handle] == NULL){
		*retval = -1;
		return EBADF;	
	}
	
	// fd was not opened for reading
	if(!(curproc->fdesc[handle]->fnode->flags != O_RDONLY || curproc->fdesc[handle]->fnode	->flags != O_RDWR)){
		*retval = -1;
		return EBADF;	
	}

	int res;
	struct uio u;
	struct iovec iov;	

	//Get the open file node
	struct openFile* curr_ofn = curproc->fdesc[handle]->fnode;
	
	// kmalloc a kernel level buffer
	void* kbuf = kmalloc(sizeof(*buf) * len);
	KASSERT(kbuf != NULL);

	lock_acquire(curproc->fdesc[handle]->fnode->filelock);
	
	//Configuration, init value
	uio_kinit(&iov, &u, (void*)kbuf, len, curr_ofn->offset, UIO_READ);

	//Called the VOP_READ function
	res = VOP_READ(curr_ofn->vNode, &u);

	if(res) {
		kfree(kbuf);
		lock_release(curproc->fdesc[handle]->fnode->filelock);
		*retval = -1;
		return res;
	}else{
		// copy user buffer to kernel buffer, and check
		size_t got;
		copyoutstr((char*)kbuf, (userptr_t) buf, len, &got);
		curr_ofn->offset = u.uio_offset;
		lock_release(curproc->fdesc[handle]->fnode->filelock);
		// length been read
		*retval = len - u.uio_resid;
		kfree(kbuf);
		return 0;
	}
}

//System write implementation
int sys_write(int handle, void * buf, size_t len, int32_t * retval) {
	// fd is not a valid file descriptor
	if(handle < 0 || handle >= OPEN_MAX || curproc->fdesc[handle] == NULL){
		*retval = -1;
		return EBADF;	
	}
	// fd was not opened for writing
	if(!(curproc->fdesc[handle]->fnode->flags != O_WRONLY || curproc->fdesc[handle]->fnode->flags != O_RDWR)){
		*retval = -1;
		return EINVAL;	
	}

	int res;
	struct uio u;
	struct iovec iov;

	//Get the open file node
	struct openFile* curr_ofn = curproc->fdesc[handle]->fnode;
	
	// kmalloc a kernel level buffer
	void* kbuf = kmalloc(sizeof(*buf) * len);
	if(kbuf == NULL) return EFAULT;
	
	size_t got;
	// check and copy user level buffer to kernel level's
	copyinstr((const_userptr_t) buf, (char*)kbuf, len, &got);
	
	lock_acquire(curproc->fdesc[handle]->fnode->filelock);	
    uio_kinit(&iov, &u, (void*)buf, len, curr_ofn->offset, UIO_WRITE);

	res = VOP_WRITE(curr_ofn->vNode, &u);

	if(res) {
		kfree(kbuf);
		lock_release(curproc->fdesc[handle]->fnode->filelock);
		* retval = -1;
		return res;
	}else{
		curr_ofn->offset = u.uio_offset;
		lock_release(curproc->fdesc[handle]->fnode->filelock);
		//length been written
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

	// oldfd is not a valid file handle
	if(curproc->fdesc[old_handle] == NULL){
		return EBADF;	
	}
	// oldfd is not a valid file handle, or newfd is a value that cannot be a valid file handle.
	if((old_handle < 0 || old_handle >= OPEN_MAX) || (new_handle < 0 || new_handle >= OPEN_MAX)){
		*retval = -1;
		return EBADF;
	}

	// dup2 itself
	if(old_handle == new_handle){
		*retval = old_handle;
		return 0;
	}

	//Check whether the destination in file descriptor table already exists
	if(curproc->fdesc[new_handle] != NULL){
		new_ofn = curproc->fdesc[new_handle]->fnode;
		//Acquire lock
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
	
	old_ofn = curproc->fdesc[old_handle]->fnode;
	curproc->fdesc[new_handle]->fnode = old_ofn;
	
	lock_acquire(curproc->fdesc[old_handle]->fnode->filelock);
	old_ofn->refCount++;
	lock_release(curproc->fdesc[old_handle]->fnode->filelock);

	*retval = new_handle;
	return 0;

}

int sys_lseek(int fd, off_t pos, int whence, off_t* retval)
{
	//fd is not a valid file handle.
	if(curproc->fdesc[fd] == NULL || fd < 0 || fd >= OPEN_MAX)
	{
		*retval = -1;
		return EBADF;
	}

	//fd refers to an object which does not support seeking
	if(!VOP_ISSEEKABLE(curproc->fdesc[fd]->fnode->vNode))
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
	VOP_STAT(curproc->fdesc[fd]->fnode->vNode, stat_ptr);

	//Acqurie lock
	lock_acquire(curproc->fdesc[fd]->fnode->filelock);
	

	if(whence==SEEK_SET)
	{
		//The resulting seek position would be negative.
		if(pos<0) return EINVAL;
		curproc->fdesc[fd]->fnode->offset = pos;
	}
	else if (whence==SEEK_CUR)
	{
		// new offset
		pos += curproc->fdesc[fd]->fnode->offset;
		//The resulting seek position would be negative.
		if(pos<0) return EINVAL;
		curproc->fdesc[fd]->fnode->offset = pos;
	}
	else if (whence==SEEK_END)
	{
		// new offset
		pos += stat_ptr->st_size;
		if(pos<0) return EINVAL;
		curproc->fdesc[fd]->fnode->offset = pos;		
	}
	kfree(stat_ptr);
	//Assign return value
	*retval = curproc->fdesc[fd]->fnode->offset;
	lock_release(curproc->fdesc[fd]->fnode->filelock);

	return 0;
}

/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
void enter_forked_process(void *tf, unsigned long addr)
{
	
	struct trapframe * tf_old = (struct trapframe *) tf;
	curproc->p_addrspace = (struct addrspace *)addr;//avoid warning
	
	struct trapframe tf_new;

	tf_old->tf_a3 = 0;
	tf_old->tf_v0 = 0;
	tf_old->tf_epc += 4;

	tf_new = *tf_old;
	// activate new addrspace
	as_activate();

	//tfnew = *tf;
	mips_usermode(&tf_new);
}

int sys_fork(struct trapframe *tf, pid_t * ret_pid)
{
	int result = 0;
	// names for new thread and new process
	const char * thread_name = "kid_thread";
	// const char * proc_name = "new_process";
	
	// create a new process
	//struct proc * new_process = proc_create(proc_name);
	
	// copy whole fd table
	// for(int i =0; i < OPEN_MAX; ++i)
	// {
	// 	new_process->fdesc[i] = kmalloc(sizeof(struct fd_table));
	// 	KASSERT(new_process->fdesc[i] != NULL);
	// 	*(new_process->fdesc[i]) = *(curproc->fdesc[i]);
	// }
	
	// copy addrspace
	// result = as_copy(curproc->p_addrspace, &(new_process->p_addrspace));
	// if(result)
	// {
	// 	return result;
	// }

	// copy trapframe
	struct trapframe *tf_child = kmalloc(sizeof(struct trapframe));
	KASSERT(tf_child != NULL);
	*tf_child = *tf;

	result = thread_fork(thread_name, NULL, &enter_forked_process, (void*)tf_child, (unsigned long)curproc->p_addrspace);
	if(result)
	{
		return result;
	}

	*ret_pid = 1;
	return 0;
}

//****************************
