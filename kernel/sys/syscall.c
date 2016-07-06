/**********************************************************************
 *							System Calls
 *
 *
 *	This file is part of Aquila OS and is released under the terms of
 *	GNU GPLv3 - See LICENSE.
 *
 *	Copyright (C) 2016 Mohamed Anwar <mohamed_anwar@opmbx.org>
 */


#include <core/system.h>
#include <core/string.h>
#include <core/arch.h>

#include <sys/proc.h>
#include <sys/sched.h>

#include <bits/errno.h>

static void sys_exit(int code __unused)
{
	printk("Recieved sys_exit\n");
	for (;;);
	//kill_process(cur_proc);
	/* We should signal the parent here */
	//kernel_idle();
}

static void sys_fork()
{
	printk("[%d] %s: fork()\n", cur_proc->pid, cur_proc->name);

	proc_t *fork = fork_proc(cur_proc);
	if (fork)
		make_ready(fork);
}

static void sys_open(const char *fn, int flags)
{
	printk("[%d] %s: open(file=%s, flags=%x)\n", cur_proc->pid, cur_proc->name, fn, flags);
	
	/* Look up the file */
	struct fs_node *node = vfs.find(vfs_root, fn);

	if (!node) {	/* File not found */
		arch_syscall_return(cur_proc, -ENOENT);
		return;
	}

	int fd = get_fd(cur_proc);	/* Find a free file descriptor */

	if (fd == -1) {		/* No free file descriptor */
		/* Reached maximum number of open file descriptors */
		arch_syscall_return(cur_proc, -EMFILE);
		return;
	}

	cur_proc->fds[fd] = (struct file) {
		.node = node,
		.offset = 0,
		.flags = flags,
	};

	int ret = node->fs->f_ops.open(&cur_proc->fds[fd]);
	
	if (ret) {	/* open returned an error code */
		arch_syscall_return(cur_proc, ret);
		release_fd(cur_proc, fd);
	} else {
		arch_syscall_return(cur_proc, fd);
	}

	return;
}

static void sys_read(int fd, void *buf, size_t count)
{
	printk("[%d] %s: read(fd=%d, buf=%x, count=%d)\n", cur_proc->pid, cur_proc->name, fd, buf, count);
	
	if (fd >= FDS_COUNT) {	/* Out of bounds */
		arch_syscall_return(cur_proc, -EBADFD);
		return;	
	}

	struct fs_node *node = cur_proc->fds[fd].node;

	if (!node) {	/* Invalid File Descriptor */
		arch_syscall_return(cur_proc, -EBADFD);
		return;
	}

	int ret = node->fs->f_ops.read(&cur_proc->fds[fd], buf, count);
	arch_syscall_return(cur_proc, ret);

	return;
}

static void sys_write(int fd, void *buf, size_t count)
{
	printk("[%d] %s: write(fd=%d, buf=%x, count=%d)\n", cur_proc->pid, cur_proc->name, fd, buf, count);
	
	if(fd >= FDS_COUNT) {	/* Out of bounds */
		arch_syscall_return(cur_proc, -EBADFD);
		return;	
	}

	struct fs_node *node = cur_proc->fds[fd].node;
	
	if (!node) {	/* Invalid File Descriptor */
		printk("Wooh!\n");
		arch_syscall_return(cur_proc, -EBADFD);
		return;
	}

	int ret = node->fs->f_ops.write(&cur_proc->fds[fd], buf, count);
	arch_syscall_return(cur_proc, ret);
	
	return;
}

static void sys_ioctl(int fd, unsigned long request, void *argp)
{
	struct fs_node *node = cur_proc->fds[fd].node;

	int ret = vfs.ioctl(node, request, argp);
	arch_syscall_return(cur_proc, ret);
}

static void sys_execve(const char *name, char *const argp[], char *const envp[])
{
	printk("[%d] %s: execve(name=%s, argp=%x, envp=%x)\n", cur_proc->pid, cur_proc->name, name, argp, envp);

	char *fn = strdup(name);
	proc_t *p = execve_proc(cur_proc, fn, argp, envp);
	kfree(fn);

	if (!p)
		arch_syscall_return(cur_proc, -1);
	else
		spawn_proc(p);
}

void (*syscall_table[])() =  {
	NULL,
	sys_exit,
	sys_fork,
	sys_open,
	sys_read,
	sys_write,
	sys_ioctl,
	sys_execve,
};