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

#define MAKING_MODULES 1
#define MODULE
#define __KERNEL__

#include <linux/types.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <asm/segment.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/dirent.h>
#include <linux/fs_struct.h>
#include <linux/proc_fs.h>
#include <linux/unistd.h>
#include <linux/sysctl.h>
#include <linux/utime.h>
#include <sys/syscall.h>

#ifndef _BASE_H_INCLUDED_
#define _BASE_H_INCLUDED_
#include "compatibility.h"
#include "extension.h"
#endif


#define LOOKUP_MKDIR	0x80000000
#define LOOKUP_CREATE	0x40000000
#define LOOKUP_NODIR	0x20000000

//4000 here is too much
#define ps 1023
#define dflags 0
//may have slash (/) at start (absolute path) and end (if dir)
#define defaultfrom "/tmp/fromdir"
#define defaultto "/tmp/todir"
#define ANYUID -1


extern char *from,*to;	//what to redirect and where
extern int flags;	//1=ro: try to prevent modifying operations on "from" (not implemented yet)
			//2=do not try to create dirs+files on the fly
			//4=turn off translucency (but keep loaded)
			//8=no merging getdents
extern int uid,gid;	//limit redirection to processes with uid/gid matching the given numbers (give one of those two)
#define no_copyonwrite	(flags&2)
#define no_translucency	(flags&4)
#define no_getdents	(flags&8)


extern char frombuf[ps], tobuf[ps];
extern struct nameidata n1,n2;
extern int from_is_subdir, to_is_subdir;
extern void* sys_call_table[];
extern struct ctl_table_header *redirection_table_header;
extern struct ctl_table redirection_table[];

#define BEGIN_KMEM {mm_segment_t old_fs=get_fs();set_fs(KERNEL_DS);
#define END_KMEM   set_fs(old_fs);}

void redirect_namei(struct nameidata *dest, const struct nameidata *src);
int have_inode(void);
int is_subdir(struct dentry *dir, struct dentry *parent);
void absolutize(char *name, struct dentry *d, struct vfsmount *m);
int redirecting_path_walk(char * name, char **endp, struct nameidata *nd, struct nameidata *nori, const struct nameidata *n1, const struct nameidata *n2);
int redirect_path(char *fname, const struct nameidata *n1, const struct nameidata *n2, int flags);
int init_vars(const char *s1, const char *s2);
int init_vars_once(void);
void release_vars(void);
void release_vars_once(void);

#define wredirect(a) redirect_path(a,&n1,&n2,dflags|LOOKUP_MKDIR|LOOKUP_CREATE)
#define dredirect(a) redirect_path(a,&n1,&n2,dflags|LOOKUP_MKDIR)
#define redirect(a) redirect_path(a,&n1,&n2,dflags)
#define unredirect(a) redirect_path(a,&n2,&n1,dflags)
#define namei_to_path(n,buf) d_path((n)->dentry, (n)->mnt, buf, ps)
