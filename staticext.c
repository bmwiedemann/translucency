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

//TODO: generalize merging getdents for layers>2
#define redirecting_sys_getdentsxx(a,orig,c)\
int a(unsigned int fd, struct c *dirp, unsigned int count)\
{\
	char buf[REDIR_BUFSIZE+1],*p;\
	int result,i;\
	struct file *f;\
	result = orig(fd, dirp, count);\
	if ((translucent_flags & no_getdents) || result) return result;\
	if ((signed)fd > current->files->max_fds) return -EBADF;\
	f = current->files->fd[fd];\
         if (!deldir(f)) return 0;\
         for (i=0; i<REDIRS; i++) {\
             if (is_valid(&redirs[i]) && \
  	         is_subdir(f->f_dentry, redirs[i].n[0].dentry) && \
	         (!is_subdir(redirs[i].n[1].dentry, redirs[i].n[0].dentry) || \
		  !is_subdir(f->f_dentry, redirs[i].n[1].dentry))) break;\
         }\
         if (i == REDIRS) return 0;\
	p = d_path(f->f_dentry, f->f_vfsmnt, buf, REDIR_BUFSIZE); memmove(buf,p,strlen(p)+1);\
	if (redirectt(&redirs[i], buf)==2) {\
		int (*orig_sys_close)(int)=sys_call_table[__NR_close];\
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

// returning 0 means executing *filename is OK... otherwise it is the final exec return value
int redirecting_execve_test(char **filename) {
	char local0[REDIR_BUFSIZE];
	int (*access)(const char *pathname, int mode)=sys_call_table[__NR_access];
	char * (*brk)(void *)=sys_call_table[__NR_brk];
	char *ret=0, *endaddr, *newendaddr;
	int result;
	if(strncpy_from_user(local0, *filename, REDIR_BUFSIZE)<0) return -EFAULT;
	if(!redirect0(local0)) return 0;
//	printk(KERN_INFO SYSLOGID ": execve %s %s\n",current->comm, local0);
	if(strcmp(current->comm, "keventd")) {	// keventd execs "/sbin/hotplug" -> brk oopses
		endaddr=brk(0);
		newendaddr=endaddr+strlen(local0)+1;
		BEGIN_KMEM
			result=access(local0,1);
		END_KMEM
		if(result) return 0;
		if(!brk || IS_ERR(brk) || (ret=brk(newendaddr))<newendaddr || IS_ERR(ret))
			return 0;
		*filename=endaddr;
	}
// this line alone happened to work on 95% of all execs, but the previous ones make it more reliable
	if(copy_to_user(*filename,local0,strlen(local0)+1)) return -EFAULT;
	return 0;
}

#if defined(__i386__)
#define fname "redirecting_sys_execve"
__asm__(
"\n.globl " SYMBOL_NAME(fname) "\n "
__ALIGN_STR"\n"
" .type " fname ",@function\n"
SYMBOL_NAME(fname) ":\n"
" pushl %ebp\n movl %esp,%ebp\n"
" leal 8(%ebp),%eax\n pushl %eax\n"
" call redirecting_execve_test\n"
" movl %ebp,%esp\n popl %ebp\n"
" testl %eax,%eax\n jz .Lexecredirorig\n ret\n"
".Lexecredirorig:\n"
" jmpl *orig_sys_execve\n"
".L" fname "_end:\n"
" .size " fname ",.L" fname "_end-" fname "\n");
#undef fname

#else
/* did some strange addl $-12,%esp
   but it is important to leave stack untouched
   that asm should work on all i386 compatible platforms
int redirecting_sys_execve(char *filename, char *const argv [], char *const envp[])
{
	int result;
	if((result=redirecting_execve_test(&filename))) return result;
	goto *orig_sys_execve;
}
*/
#endif

#if defined(__NR_open)
int redirecting_sys_open(const char *pathname, int oflags, mode_t mode)
{
	char local0[REDIR_BUFSIZE];
	int extraflags=LOOKUP_NODIR, redirflags=oflags, rresult;
	if((oflags&O_CREAT) || (oflags&O_WRONLY) || (oflags&O_RDWR)|| (oflags&O_APPEND)) {
		extraflags|=LOOKUP_CREATE|LOOKUP_MKDIR;
		if(oflags&O_TRUNC) extraflags|=LOOKUP_TRUNCATE;
	}
	//TODO: consider O_EXCL|O_CREAT here
	if(strncpy_from_user(local0, pathname, REDIR_BUFSIZE)<0) return -EFAULT;
	if((rresult=redirect_path(local0,0, dflags | extraflags))) {
		int result;
		BEGIN_KMEM
			result = orig_sys_open(local0, redirflags, mode);
		END_KMEM
		if(no_fallback(result)) return result;
	}
	return orig_sys_open(pathname, oflags, mode);
}
#endif

#if defined(__NR_chdir)
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
#endif

#if defined(__NR_symlink)
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
		if(no_fallback(result)) return result;
	}
	return orig_sys_symlink(oldpath, newpath);
}
#endif


#if defined(__NR_socketcall)
#include <linux/un.h>
/// catch bind+connect syscall for named AF_UNIX sockets - needed for syslog, gpm, nscd and resmgr
int redirecting_sys_socketcall(int call, unsigned long *args)
{
        if(call==SYS_BIND || call==SYS_CONNECT) {
		struct sockaddr sa;
		unsigned long largs[3];
		if(copy_from_user(largs, args, sizeof(largs))) return -EFAULT;
		if(copy_from_user(&sa, (void *)largs[1], sizeof(sa))) return -EFAULT;
		if(sa.sa_family==AF_UNIX) {
			struct sockaddr_un local1;
			char local0[REDIR_BUFSIZE];
			unsigned int len2;
			largs[2]-=sizeof(local1.sun_family);
			if(largs[2]>sizeof(local1.sun_path)) largs[2]=sizeof(local1.sun_path);
			if(copy_from_user(local0, &(((struct sockaddr_un*)largs[1])->sun_path), largs[2])) return -EFAULT;
			local0[largs[2]]=0;
//			printk("redirecting process %s %lu %lX %lu socket %s ",current->comm, largs[0], largs[1], largs[2], local0);
			if(local0[0] && dredirect0(local0) && (len2=strlen(local0))<sizeof(local1.sun_path)) {
				int result;
				largs[1]=(unsigned long)(&local1);
				local1.sun_family=sa.sa_family;
				largs[2]=len2+sizeof(sa.sa_family);
				strncpy(local1.sun_path, local0, sizeof(local1.sun_path));
	                	BEGIN_KMEM
					result=orig_sys_socketcall(call, largs);
				END_KMEM
//				printk("to %s %lu -> return %i\n", local0, largs[2], result);
				return result;
			}
//			printk("\n");
		}
	}
	return orig_sys_socketcall(call, args);
}
#endif

