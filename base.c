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

/* There are only 8 redirections possible */
struct translucent redirs[8];
int translucent_uid   = ANYUID;
int translucent_gid   = ANYUID;
int translucent_flags = 0;
int translucent_cnt   = 0;

inline int match_uids(void) {
	uid_t (*geteuid)(void)=sys_call_table[SYS_geteuid];
	gid_t (*getegid)(void)=sys_call_table[SYS_getegid];
	return ((translucent_uid == ANYUID || translucent_uid == geteuid()) && 
	        (translucent_gid == ANYUID || translucent_gid == getegid()));
}

void redirect_namei(struct nameidata *dest, const struct nameidata *src) {
	mntget(src->mnt);  dget(src->dentry);
	mntput(dest->mnt); dput(dest->dentry);
	*dest = *src;
}

void copy_namei(struct nameidata *dest, const struct nameidata *src) {
	mntget(src->mnt);  dget(src->dentry);
	*dest = *src;
}

inline int match_namei(const struct nameidata *n1, const struct nameidata *n2) {
	return (n1->dentry == n2->dentry);
}

inline int have_inode(struct nameidata *n) { 
	return /*n!=NULL && */ n->dentry!=NULL && n->dentry->d_inode!=NULL;
}

inline int is_special(struct nameidata *n) {
	return have_inode(n) && (!S_ISREG(n->dentry->d_inode->i_mode) 
		|| n->dentry->d_inode->i_sb->s_magic==PROC_SUPER_MAGIC);
}

int mycopy(struct nameidata *nd, struct nameidata *nnew) {
	char *p,buf[REDIR_BUFSIZE+1];
	ssize_t (*sys_write)(int fd, const void *buf, size_t count)=sys_call_table[SYS_write];	
	ssize_t (*sys_read)(int fd, void *buf, size_t count)=sys_call_table[SYS_read];
	int (*sys_close)(int fd)=sys_call_table[SYS_close];
	int result,inphandle,outphandle,mode=nd->dentry->d_inode->i_mode;

// exclude device/pipe/socket/dir and proc entries from COW
	if(is_special(nd)) return -ENODEV;

	mode &= 01777;
	p = d_path(nd->dentry, nd->mnt, buf, REDIR_BUFSIZE);

//	printk(KERN_INFO "redir-copy-on-write: %s %o\n",p,mode);

	BEGIN_KMEM
		result=orig_sys_open(p,O_RDONLY,0666);
	END_KMEM
	if(result<0) return result;
	inphandle=result;

	p=d_path(nnew->dentry, nnew->mnt, buf, REDIR_BUFSIZE);
	BEGIN_KMEM
		result=orig_sys_open(p,O_WRONLY|O_CREAT,mode);
	END_KMEM
	if(result<0) return result;
	outphandle=result;

	BEGIN_KMEM
		while((result=sys_read(inphandle,buf,REDIR_BUFSIZE))>0&&sys_write(outphandle,buf,result)>0);
	END_KMEM

	sys_close(inphandle);
	sys_close(outphandle);
	return 0;
}

int mymkdir(struct nameidata *nd, struct nameidata *n, int mode, struct translucent *t) {
	char *p,buf[REDIR_BUFSIZE+1];
	int result;
	if ((translucent_flags&no_copyonwrite)) return -1;
	mode &= 07777;
	p = namei_to_path(nd, buf);
	memmove(buf,p,strlen(p)+1);
//	printk(KERN_INFO "redir-mkdir: %s %o\n",buf,mode);
	if(redirect(t,buf)) {
		BEGIN_KMEM
			result=orig_sys_mkdir(buf,mode);
		END_KMEM
		path_init(buf,nd->flags,n);path_walk(buf,n); //re-init nd
		return result;
	}
	return -1;
}

int mymkdir2(struct nameidata *nd, struct nameidata *n, struct translucent *t) {
	return mymkdir(nd,n,nd->dentry->d_inode->i_mode,t);
}

void absolutize(char *name, struct dentry *d, struct vfsmount *m) {
	char *p,buf[REDIR_BUFSIZE+1];
	int l,l2 = strlen(name);
	if(*name == '/') return;
	p = d_path(d, m, buf, REDIR_BUFSIZE);
	l = strlen(p);
	if (l+l2 >= REDIR_BUFSIZE-3) return; //TODO put better return value here?
	memmove(&name[l+1], name, l2+1);
	memcpy(name, p, l);
	name[l] = '/';
}

int redirect_path_init(char *name, unsigned int flags, 
		       struct nameidata *nd, struct nameidata *nori, 
		       const struct nameidata *n1, const struct nameidata *n2,
		       struct translucent *t)
{
	char *np=name;
	int haddotdot=0;
	if (path_init(name,flags,nd)==0) return 0;
	if (*name!='/'){		// && is_subdir(nd->dentry,n1->dentry)) {
		absolutize(name,nd->dentry,nd->mnt);
		path_release(nd);
		path_init(name,flags,nd);
		//printk(KERN_INFO "now having %s\n",name);
	}
	if (nori) copy_namei(nori,nd);

//this is neccessary to let .. work in pathnames
	while(1) {
		char *p=strstr(np,"/..");
		if (p==NULL) break;
		if (*(p+3)!=0 && *(p+3)!='/') np=p+3;
		else {
			char *dotdot=p;
			while (--p>=name && *p!='/');
			if (p<name) break;
			memmove(p,dotdot+3,strlen(dotdot)-2);
	// imagine /sub1/sub2/../../test
	//                      ^dotdot+3
	//                   ^dotdot
	//              ^p
	//         ^name
			np=name;
			haddotdot=1;
		}
	}
	if (haddotdot) {
		redirect_path (name,t,n2,n1,dflags);
		//printk(KERN_INFO "now having %s\n",name);
	}
	return 1;
}

//same as path walk: replaces any occurrence of n1 by n2
int redirect_path_walk(char *name, char **endp, 
		       struct nameidata *nd, struct nameidata *nori, 
		       const struct nameidata *n1, const struct nameidata *n2, 
		       struct translucent *t) 
{
	char savedchar=0,*slash=name,*np=name,*lastnp=NULL;
	int error1=0,error2=0,valid1=1,valid2=1,lflags=nd->flags;
	nd->flags &= 0x1fffffff;

	while (np && (valid1 || valid2)) {
		if (valid2 && match_namei(nd,n1)) redirect_namei(nd,n2);
		slash = strchr(np,'/');
		if (slash) { savedchar = *(++slash); *slash=0; }
		if (valid1) { 
			error1 = path_walk(np,nori);
			if (!error1 && !have_inode(nori)) { path_release(nori); error1 = -1; }
			if (error1) { valid1=0; }
		}
		if (valid2) {
			error2 = path_walk(np, nd);
			if (error2 && slash && valid1 && (lflags & LOOKUP_MKDIR) && 
			    mymkdir2(nori,nd,t)==0) 
				error2 = 0;
			if (!error2 && !have_inode(nd)) {
//if(valid1 && slash && (lflags&LOOKUP_MKDIR) && mymkdir2(nori,nd)==0);else 
				if (valid1 && !slash && (lflags & LOOKUP_CREATE) && !(translucent_flags&no_copyonwrite)) {
					if (mycopy(nori,nd) == -ENODEV) { 
						path_release(nd);
						error2 = -1;
					}
				}
			} //TODO: verify

			if (error2) valid2 = 0;
		}
		if (slash) { *slash = savedchar; }

/*		if(*np=='.' && *(np+1)=='.' && (*(np+2)==0 || *(np+2)=='/')) {
//NOT working yet with symbolic links
//			char buf[REDIR_BUFSIZE+1],*p;
			if(!nd1 && match_namei(nd2,n2)) {nd1=nori;copy_namei(nd1,n1);}
			if(!nd2 && match_namei(nd1,n1)) {nd2=nd;copy_namei(nd2,n2);}

			p=namei_to_path(nd,buf);
			if(redirect_path(p,t,n2,n1,dflags)) {
				strncat(p,"/",REDIR_BUFSIZE);
				if(slash) strncat(p,slash,REDIR_BUFSIZE);
				path_release(nd);
				printk(KERN_INFO "redir %s - %s - %s\n",np,name,p);

				redirect_path_init(p,lflags,nd,nori,n1,n2,t);
				return redirect_path_walk(p,endp,nd,nori,n1,n2,t);
			}
		}
*/

		if (valid2 && match_namei(nd,n1)) redirect_namei(nd,n2);
//		if((lflags & LOOKUP_MKDIR) && !error1 && error2 && valid1 && have_inode(nori)) //have to create dir (echo abc > static/test)
//		{ error2=mymkdir(nd2,nd1->dentry->d_inode->i_mode); }
		lastnp=np;
		np=slash;
	}
	if(valid1 && (!valid2 || !have_inode(nd))) {
		if((lflags&LOOKUP_NOSPECIAL) && is_special(nori)) {
			path_release(nori); valid1=0;
			if(valid2) { path_release(nd); valid2=0; }
		} // else TODO: better place for copyonwrite here?
	}
	if (!valid2 && valid1) {*nd=*nori;valid1=0;}
	if (valid1) {
//		char buf[REDIR_BUFSIZE],buf2[REDIR_BUFSIZE];
//if(valid2)printk("<1>debug: 1 %X %i %s %s\n",lflags, valid1+2*valid2,namei_to_path(nori,buf),namei_to_path(nd,buf2));
		if (nd->dentry==nori->dentry) {path_release(nd);valid2=0;}
		path_release(nori);
		if ((lflags&LOOKUP_NODIR) && valid2 && is_subdir(nd->dentry,n2->dentry) && 
		    nd->dentry->d_inode && S_ISDIR(nd->dentry->d_inode->i_mode)) {
			path_release(nd);
			valid2=0;
		}
	}
	if (endp) *endp=(slash?lastnp:NULL);
	return (!valid2);
}

/* if 0 is returned no redirection could be done... 
   either there was nothing to redirect or other errors occurred (maybe reflect this in return status later)
   if 1 is returned fname will be some pathname
   if 2 is returned fname has been redirected (from n1 to n2)
*/
int redirect_path(char *fname, struct translucent *t, const struct nameidata *n1, const struct nameidata *n2, int rflags) 
{
	char buf[REDIR_BUFSIZE+1],*p,*p2;
	int i,l,error=1,result=0;
	struct nameidata n,nori;
	if ((translucent_flags & no_translucency) || !match_uids()) return 0;
	if (t == NULL) {
		for (i=0; i<8; i++) {
			if (redirs[i].valid == valid_translucency) {
				t = &redirs[i];
				n1 = &t->n1;
				n2 = &t->n2;
				redirect_path_init(fname,rflags,&n,&nori,n1,n2,t);
				error = redirect_path_walk(fname,&p2,&n,&nori,n1,n2,t);
				if (!error) break;
			}
		}
		if (i==8) { goto out_release; }
	} else {
		redirect_path_init(fname,rflags,&n,&nori,n1,n2,t);
		error = redirect_path_walk(fname,&p2,&n,&nori,n1,n2,t);
		if (error) goto out_release;
	}
	// if(!is_subdir(n.dentry,n2->dentry)) goto out_release;
	p = d_path(n.dentry, n.mnt, buf, REDIR_BUFSIZE);
	l = strlen(p);
	//strncat(p,p2,REDIR_BUFSIZE-l+(buf-p));
	//if(is_subdir(n.dentry,n2->dentry)||is_subdir(n.dentry,n1->dentry))printk(KERN_INFO "o: %i %i %s - %s\n",error,l,p,p2);
	if (l >= REDIR_BUFSIZE-2) goto out_release;
	memcpy(fname,p,l+1);
	result=1;
	if (is_subdir(n.dentry,n2->dentry)) ++result;
out_release:
	if (!error) { path_release(&n); }
	return result;
}

static void cleanup_translucent(struct translucent *t) 
{
	if (is_valid(t)) {
		path_release(&t->n1);
		path_release(&t->n2);
		t->valid = 0;
		memset(t->from, 0, sizeof(t->from));
		memset(t->to,   0, sizeof(t->to));
		memset(t->b,    0, sizeof(t->b));
		MOD_DEC_USE_COUNT; translucent_cnt--;
	}
}

static int init_translucent(struct translucent *t) 
{
	int error;

	if (sscanf(t->b, "%s -> %s", t->from, t->to) != 2) {
		if (t->b[0] == 0 || isspace(t->b[0])) {
			// already cleaned? yes! -- cleanup_translucent(t);
			printk(KERN_INFO SYSLOGID ": mapping %d removed\n", (int)t->index);
			return 0;
		} else {
			printk(KERN_ERR SYSLOGID ": bad mapping %d `%s'\n", (int)t->index, t->b);
			return -ENOENT;
		}
	}

	path_init(t->from, dflags, &t->n1);
	path_init(t->to,   dflags, &t->n2);

	error = path_walk(t->from, &t->n1);
	if (error) return -ENOENT;

	error = path_walk(t->to, &t->n2);
	if (error || !have_inode(&t->n2)) {
		path_release(&t->n1);
		return -ENOENT;
	}

	if (is_subdir(t->n1.dentry, t->n2.dentry))
		t->flags |= from_is_subdir;

	if (is_subdir(t->n2.dentry, t->n1.dentry))
		t->flags |= to_is_subdir;

	printk(KERN_INFO SYSLOGID ": mapping %d established %s -> %s\n", (int)t->index, t->from, t->to);
	t->valid = valid_translucency;
	MOD_INC_USE_COUNT; translucent_cnt++;
	return 0;
}

static int redir_handler(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp) 
{
	int error=0;
	char backup[REDIR_BUFSIZE+1];
	int num = table->ctl_name - CTL_ENTRY_BASE;
	strncpy(backup, table->data, REDIR_BUFSIZE);
	error = proc_dostring(table,write,filp,buffer,lenp);
	if (write && !error) {
		cleanup_translucent(&redirs[num]);
		error = init_translucent(&redirs[num]);
		if (error) { 
			printk(KERN_ERR SYSLOGID ": could not setup %s\n",(char *)table->data);
			strncpy(table->data, backup, REDIR_BUFSIZE);
			error = init_translucent(&redirs[num]);
		}
	}
	return error;
}

static int proc_doint_ro(ctl_table *table, int write, struct file *filp,
			 void *buffer, size_t *lenp) 
{
	int *i;
	char buf[1024];
	size_t left, len;
	if (!table->data || !table->maxlen || !*lenp || (filp->f_pos && !write)) {
		*lenp = 0;
		return 0;
	}
	if (write) return 0;
	i = (int *) table->data;
	left = *lenp;
	sprintf(buf, "%d\n", (*i));
	len = strlen(buf);
	if (copy_to_user(buffer, buf, len))
		return -EFAULT;
	left -= len;
	buffer += len;
	*lenp -= left;
	filp->f_pos += *lenp;
	return 0;
}


#define REDIR_ENTRY(n) {CTL_ENTRY_BASE+n,#n,&redirs[n].b,REDIR_BUFSIZE-1,0644,NULL,&redir_handler}
struct ctl_table redirection_dir_table[] = {
	{CTL_TABLE_BASE+1, "uid",   &translucent_uid,   sizeof(translucent_uid),   0644, NULL, &proc_dointvec},
	{CTL_TABLE_BASE+2, "gid",   &translucent_gid,   sizeof(translucent_gid),   0644, NULL, &proc_dointvec},
	{CTL_TABLE_BASE+3, "flags", &translucent_flags, sizeof(translucent_flags), 0644, NULL, &proc_dointvec},
	{CTL_TABLE_BASE+4, "used",  &translucent_cnt,   sizeof(translucent_cnt),   0444, NULL, &proc_doint_ro},
	REDIR_ENTRY(0), 
	REDIR_ENTRY(1),
	REDIR_ENTRY(2),
	REDIR_ENTRY(3),
	REDIR_ENTRY(4),
	REDIR_ENTRY(5),
	REDIR_ENTRY(6),
	REDIR_ENTRY(7),
	{0}
};
struct ctl_table_header *redirection_table_header;
struct ctl_table redirection_table[] = {
	{CTL_TABLE_BASE, "translucency", NULL, 0, 0555, redirection_dir_table},
	{0}
};

int __init translucent_init_module(void) 
{
	int i;
	memset(&redirs, 0, sizeof(redirs));
	for (i=0; i<8; i++) redirs[i].index=i;
	init_redir_calltable();
	redirection_table_header = register_sysctl_table(redirection_table, 0);
	return 0;
}

void __exit translucent_cleanup_module(void) 
{
	restore_redir_calltable();
	unregister_sysctl_table(redirection_table_header);
}

MODULE_AUTHOR("See http://sourceforge.net/projects/translucency/");
MODULE_DESCRIPTION("translucency/redirection");
MODULE_LICENSE("GPL");
module_init(translucent_init_module);
module_exit(translucent_cleanup_module);
