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

#include <linux/types.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <asm/segment.h>
#include <asm/unistd.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/dirent.h>
#include <linux/fs_struct.h>
#include <linux/proc_fs.h>
#include <linux/unistd.h>
#include <linux/sysctl.h>
#include <linux/utime.h>
#include <linux/ctype.h>

#ifndef _BASE_H_INCLUDED_
#define _BASE_H_INCLUDED_
#include "compatibility.h"
#include "extension.h"
#endif

#define LOOKUP_MKDIR	0x80000000
#define LOOKUP_CREATE	0x40000000
#define LOOKUP_NODIR	0x20000000
#define LOOKUP_NOSPECIAL	0x20000000
#define LOOKUP_TRANSLUCENCY_MASK 0x1fffffff

// 2000 here is too much, since kernel stack is small and mymkdir does one recursion
#define REDIR_BUFSIZE 1023
#define dflags 0
#define ANYUID -1
#define SYSLOGID "translucency"
#define REDIRS 1
#define MAX_LAYERS 2

struct translucent {
#define valid_translucency  (0x77abc713)
#define is_valid(t) ((t)->valid == valid_translucency)
	unsigned long valid;
	char flags;
	int index; /* up to REDIRS possible */
	int layers;
	struct nameidata n[MAX_LAYERS];
	char b[REDIR_BUFSIZE];
};

#define CTL_TABLE_BASE 0x89194729
#define CTL_TABLE_STATIC 6
#define CTL_ENTRY_BASE (CTL_TABLE_BASE+CTL_TABLE_STATIC)

extern void* sys_call_table[];
extern struct translucent redirs[REDIRS];
extern int translucent_uid;
extern int translucent_gid;
extern int translucent_flags;

#define no_translucency     (1<<0)
#define no_copyonwrite      (1<<1)
#define no_getdents         (1<<2)
#define do_downcase         (1<<3)

extern struct ctl_table_header *redirection_table_header;
extern struct ctl_table redirection_table[];

#define BEGIN_KMEM {mm_segment_t old_fs=get_fs();set_fs(KERNEL_DS);
#define END_KMEM   set_fs(old_fs);}

void redirect_namei      (struct nameidata *dest, const struct nameidata *src);
int  have_inode          (struct nameidata *);
int  is_special          (struct nameidata *);
int  is_subdir           (struct dentry *dir, struct dentry *parent);
void absolutize          (char *name, struct dentry *d, struct vfsmount *m);
int  redirect_path_walk  (char *name, char **endp, struct nameidata *n, struct translucent *t);
int  redirect_path       (char *fname, struct translucent *t, int flags);

void init_redir_calltable    (void);
void restore_redir_calltable (void);


static inline int wredirectt(struct translucent *t, char *a) { return redirect_path(a,t,dflags|LOOKUP_MKDIR|LOOKUP_CREATE); }
static inline int dredirectt(struct translucent *t, char *a) { return redirect_path(a,t,dflags|LOOKUP_MKDIR); }
static inline int  redirectt(struct translucent *t, char *a) { return redirect_path(a,t,dflags); }

static inline int wredirect0(char *a) { return redirect_path(a,0,dflags|LOOKUP_MKDIR|LOOKUP_CREATE); }
static inline int dredirect0(char *a) { return redirect_path(a,0,dflags|LOOKUP_MKDIR); }
static inline int  redirect0(char *a) { return redirect_path(a,0,dflags); }

static inline char *namei_to_path(struct nameidata *n, char *buf) 
{ return d_path((n)->dentry, (n)->mnt, buf, REDIR_BUFSIZE); }

static inline int no_fallback(int error)	// return false if fallback to original filename is wanted
{ return (error!=-ENOENT && error!=-EROFS); }

#ifndef MODULE_LICENSE
#define MODULE_LICENSE(name)
#endif
