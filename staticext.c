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
*/

static inline void convert_dirent64(struct dirent *out, const struct dirent64 *in)
{
	printk(SYSLOGID ": TODO convert_dirent64\n");
	out->d_reclen=in->d_reclen;
	out->d_off=in->d_off;
	out->d_ino=in->d_ino;
	//memcpy(out->d_name, in->d_name, NAME_MAX+1??);
}

#define redirecting_sys_getdentsxx(fname,orig,dtype)\
int fname(unsigned int fd, struct dtype *dirp, unsigned int count)\
{\
	int result, readresult;\
	ssize_t (*sys_read)(int fd, void *buf, size_t count)=sys_call_table[__NR_read];\
	result = orig(fd, dirp, count);\
	if ((translucent_flags & no_getdents) || result>=0) return result;\
	if (result==-ENOTDIR /* TODO: somehow check that it is our file*/) {\
		struct dtype temp, *p=&temp;\
		struct dirent64 inbuf, *in=&inbuf;\
		char *out=(char *)dirp;\
		\
		/*printk(SYSLOGID ": getdents fd %i count %i\n", fd, count);*/\
		count-=sizeof(struct dirent64);\
		result=0;\
		while((unsigned)result<=count) {\
			BEGIN_KMEM\
				readresult=sys_read(fd, in, sizeof(struct dirent64));\
			END_KMEM\
			if(readresult<0) return readresult;\
			if(readresult==0) break;\
			/*printk(SYSLOGID ": getdents %i %i %s\n", readresult, in->d_reclen, in->d_name);*/\
			if(sizeof(struct dtype)!=sizeof(struct dirent64)) convert_dirent64((struct dirent*)p, in);\
			else p=(struct dtype*) in;\
			if(copy_to_user(out, p, p->d_reclen)) return -EFAULT;\
			out+=p->d_reclen;\
			result+=p->d_reclen;\
		}\
	}\
	/*printk(SYSLOGID ": getdents return %i\n", result);*/\
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
	int result, rresult;
	if(strncpy_from_user(local0, *filename, REDIR_BUFSIZE)<0) return -EFAULT;
	if((rresult=redirect0(local0))<=0) return rresult;
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
	int extraflags=LOOKUP_OPEN|LOOKUP_NOSPECIAL, redirflags=oflags, rresult;
	if((oflags&O_CREAT) || (oflags&O_WRONLY) || (oflags&O_RDWR)|| (oflags&O_APPEND)) {
		extraflags|=LOOKUP_CREATE|LOOKUP_MKDIR;
		if(oflags&O_TRUNC) extraflags|=LOOKUP_TRUNCATE;
	}
	if((oflags&O_CREAT)) extraflags|=LOOKUP_CREATES;
	//TODO: consider O_EXCL|O_CREAT here
	if(strncpy_from_user(local0, pathname, REDIR_BUFSIZE)<0) return -EFAULT;
	if((rresult=redirect2(local0, dflags | extraflags))>0) {
		int result;
                if(rresult>>4) {
                        return (rresult>>4)-1;
                }
		BEGIN_KMEM
			if(rresult&4) orig_sys_unlink(local0);
			result = orig_sys_open(local0, redirflags, mode);
			if(result<0 && rresult&4) translucent_create_whiteout(local0);
		END_KMEM
		if(no_fallback(result)) return result;
	}
	if(rresult<0) return rresult;
	return orig_sys_open(pathname, oflags, mode);
}
#endif
#if defined(__NR_creat)
int redirecting_sys_creat(const char *pathname, mode_t mode)
{
	return redirecting_sys_open(pathname, O_WRONLY|O_CREAT|O_TRUNC, mode);
}
#endif

#if defined(__NR_chdir)
int redirecting_sys_chdir(const char *path)
{
	char local0[REDIR_BUFSIZE];
	int result, rresult;
	if(strncpy_from_user(local0, path, REDIR_BUFSIZE)<0) return -EFAULT;
	rresult=redirect0(local0);
	if(rresult<0) return rresult;
	result = orig_sys_chdir(path);
	if(result>=0) return result;
	if(rresult) {
		BEGIN_KMEM
			result = orig_sys_chdir(local0);
		END_KMEM
	}
	return result;
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
			int rresult=0;
			unsigned int len2;
			largs[2]-=sizeof(local1.sun_family);
			if(largs[2]>sizeof(local1.sun_path)) largs[2]=sizeof(local1.sun_path);
			if(copy_from_user(local0, &(((struct sockaddr_un*)largs[1])->sun_path), largs[2])) return -EFAULT;
			local0[largs[2]]=0;
//			printk("redirecting process %s %lu %lX %lu socket %s ",current->comm, largs[0], largs[1], largs[2], local0);
			if(local0[0] && (rresult=redirect2(local0, LOOKUP_MKDIR|LOOKUP_CREATES))>0 && (len2=strlen(local0))<sizeof(local1.sun_path)) {
				int result;
				largs[1]=(unsigned long)(&local1);
				local1.sun_family=sa.sa_family;
				largs[2]=len2+sizeof(sa.sa_family);
				strncpy(local1.sun_path, local0, sizeof(local1.sun_path));
				if(rresult&4) BEGIN_KMEM orig_sys_unlink(local0); END_KMEM
	                	BEGIN_KMEM
					result=orig_sys_socketcall(call, largs);
				END_KMEM
				if(result<0 && rresult&4) translucent_create_whiteout(local0);
//				printk("to %s %lu -> return %i\n", local0, largs[2], result);
				return result;
			}
			if(rresult<0) return rresult;
//			printk("\n");
		}
	}
	return orig_sys_socketcall(call, args);
}
#endif

