/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2001 Bernhard M. Wiedemann - All Rights Reserved
 *
 *   This file is part of the translucency kernel module.
 *   translucency is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License
 *   see file COPYING for more details
 *
 * ----------------------------------------------------------------------- */

#include "base.h"
/* this file contains manually generated syscall-redirection functions

   note: this data structure is only an sorry excuse of a dynamic thing:
	 those add and del functions will have problems with SMP and anyway
   TODO: have to be rewritten
*/

#define maxdirs 16
struct file *redirected_directories[maxdirs];
int dircount=0;
int adddir(struct file *newdir) {if(dircount>=maxdirs)return 1;redirected_directories[dircount++]=newdir;return 0;}
int deldir(struct file *dir) {
	int i;
	for(i=0;i<dircount;++i) if(redirected_directories[i]==dir) {
		if(i< --dircount)
		memmove(redirected_directories[i],
			redirected_directories[i+1],
			sizeof(struct file *)*(dircount-i));
		return 0;
	}
	return 1;
}

#define redirecting_sys_getdentsxx(a,orig,c)\
int a(unsigned int fd, struct c *dirp, unsigned int count)\
{\
	char buf[REDIR_BUFSIZE+1],*p;\
	int result,i;\
	struct file *f;\
	result = orig(fd, dirp, count);\
	if ((translucent_flags & no_getdents) || result) return result;\
	if (fd > current->files->max_fds) return -EBADF;\
	f = current->files->fd[fd];\
         if (!deldir(f)) return 0;\
         for (i=0; i<REDIRS; i++) {\
                  if (is_valid(&redirs[i])) {\
  	                 if (!is_subdir(f->f_dentry, redirs[i].n1.dentry) || \
	                     ((redirs[i].flags & to_is_subdir) && \
		                is_subdir(f->f_dentry, redirs[i].n2.dentry))) continue;\
                          break;\
                  }\
         }\
         if (i == REDIRS) return 0;\
	p = d_path(f->f_dentry, f->f_vfsmnt, buf, REDIR_BUFSIZE); memmove(buf,p,strlen(p)+1);\
	if (redirect(&redirs[i], buf)) {\
		int (*orig_sys_close)(int)=sys_call_table[SYS_close];\
		BEGIN_KMEM\
			result = orig_sys_open(buf, O_RDONLY, 0644);\
		END_KMEM\
		if(result<0) return 0;\
		current->files->fd[fd] = current->files->fd[result];\
		current->files->fd[result] = f;\
		adddir(current->files->fd[fd]);\
		orig_sys_close(result);\
		result = orig(fd, dirp, count);\
	}\
	return result;\
}

redirecting_sys_getdentsxx(redirecting_sys_getdents,orig_sys_getdents,dirent)
redirecting_sys_getdentsxx(redirecting_sys_getdents64,orig_sys_getdents64,dirent64)

int redirecting_execve_test(char **filename) {
	char local0[REDIR_BUFSIZE];
	if(strncpy_from_user(local0, *filename, REDIR_BUFSIZE)<0) return -EFAULT;
	if(redirect0(local0)) {
		int (*access)(const char *pathname, int mode)=sys_call_table[SYS_access];
		int result;
//		printk("execve-redir: %s\n",local0);
		BEGIN_KMEM
			result=access(local0,1);
		END_KMEM
		if(result==0) {
			char * (*brk)(void *)=sys_call_table[SYS_brk];
			char *endaddr=brk(0),*newendaddr=endaddr+strlen(local0)+1;
			if(brk(newendaddr)<newendaddr) return 0;
			*filename=endaddr;
// this line alone happened to work on 95% of all exec's, but those other 4 make it more reliable
			if(copy_to_user(*filename,local0,strlen(local0)+1)) return -EFAULT;
		}
	}
	return 0;
}

/* did some strange addl $-12,%esp
that asm should work on all i386 compatible platforms 
int redirecting_sys_execve(const char *filename, char *const argv [], char *const envp[])
{
	int result;
	if((result=redirecting_execve_test(&filename))) return result;
	goto *orig_sys_execve;
}
*/

__asm__(
"\n"__ALIGN_STR"\n"
SYMBOL_NAME_STR(redirecting_sys_execve) ":\n\t"
"pushl %ebp\n\tmovl %esp,%ebp\n\t"
"leal 8(%ebp),%eax\n\tpushl %eax\n\t"
"call redirecting_execve_test\n\t"
"movl %ebp,%esp\n\tpopl %ebp\n\t"
"testl %eax,%eax\n\tjz execredirorig\n\tret\n"
"execredirorig:\n\t"
"jmpl *orig_sys_execve\n");


int redirecting_sys_open(const char *pathname, int oflags, mode_t mode)
{
	char local0[REDIR_BUFSIZE];
	int extraflags=LOOKUP_NODIR, redirflags=oflags, rresult;
	if((oflags&O_CREAT) || (oflags&O_WRONLY) || (oflags&O_RDWR)|| (oflags&O_APPEND)) {
		extraflags|=LOOKUP_MKDIR;
		if(!(oflags&O_TRUNC)) extraflags|=LOOKUP_CREATE|LOOKUP_MKDIR;
	}
	//TODO: consider O_EXCL|O_CREAT here
	if(strncpy_from_user(local0, pathname, REDIR_BUFSIZE)<0) return -EFAULT;
	if((rresult=redirect_path(local0,0,0,0,dflags | extraflags))) {
		int result;
		if(rresult>1 && (extraflags&LOOKUP_MKDIR)) redirflags|=O_CREAT; //makes clever gnu cp work which omits O_CREAT flag if previous stat returned 0
		BEGIN_KMEM
			result = orig_sys_open(local0, redirflags, mode);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_open(pathname, oflags, mode);
}

int redirecting_sys_chdir(const char *path)
{
	char local0[REDIR_BUFSIZE];
	int result = orig_sys_chdir(path);
	if(result>=0) return result;
	if(strncpy_from_user(local0, path, REDIR_BUFSIZE)<0) return -EFAULT;
	if(redirect0(local0)) {
		BEGIN_KMEM
			result = orig_sys_chdir(local0);
		END_KMEM
	}
	return result;
}

int redirecting_sys_symlink(const char *oldpath, const char *newpath)
{
	char local0[REDIR_BUFSIZE];
	char local1[REDIR_BUFSIZE];
	int rresult;
	if(strncpy_from_user(local1, newpath, REDIR_BUFSIZE)<0) return -EFAULT;
	if((rresult=dredirect0(local1))>0) {
		int result;	if(strncpy_from_user(local0, oldpath, REDIR_BUFSIZE)<0)return -EFAULT; //redirect(local0);
//FIXME		if(local0[0]!='/' && is_subdir(current->fs->pwd,n1.dentry) && rresult>1) absolutize(local0,current->fs->pwd, current->fs->pwdmnt);
		BEGIN_KMEM
			result = orig_sys_symlink(local0, local1);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_symlink(oldpath, newpath);
}

int redirecting_sys_utime(const char *filename, const struct utimbuf *buf)
{
	char local0[REDIR_BUFSIZE];
	struct utimbuf local1;
	if(strncpy_from_user(local0, filename, REDIR_BUFSIZE)<0) return -EFAULT;
	if(wredirect0(local0)) {
		void *plocal1=NULL;
		int result;	if(buf && copy_from_user((plocal1=&local1), buf, sizeof(local1))){return -EFAULT;}
		BEGIN_KMEM
			result = orig_sys_utime(local0, plocal1);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_utime(filename, buf);
}
