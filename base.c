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
MODULE_AUTHOR("Bernhard M. Wiedemann (b.m.w@gmx.net)");
MODULE_DESCRIPTION("translucency/redirection");
MODULE_LICENSE("GPL");
//MODULE_PARM_DESC(uid,"i","whom to redirect")
MODULE_PARM(from,"s");
MODULE_PARM(to,"s");
MODULE_PARM(uid,"i");
MODULE_PARM(gid,"i");
MODULE_PARM(flags,"i");

char *from=NULL,*to=NULL;
int flags=0,uid=ANYUID,gid=ANYUID;

char frombuf[ps]=defaultfrom, tobuf[ps]=defaultto;
struct nameidata n1,n2;
int from_is_subdir, to_is_subdir;

int match_uids(void) {
	uid_t (*geteuid)(void)=sys_call_table[SYS_geteuid];
	gid_t (*getegid)(void)=sys_call_table[SYS_getegid];
	return ((uid==ANYUID || uid==geteuid())
		&& (gid==ANYUID || gid==getegid()));
}

void redirect_namei(struct nameidata *dest, const struct nameidata *src) {
	mntget(src->mnt);dget(src->dentry);
	mntput(dest->mnt);dput(dest->dentry);
	*dest=*src;
}
void copy_namei(struct nameidata *dest, const struct nameidata *src) {
	mntget(src->mnt);dget(src->dentry);
	*dest=*src;
}

inline int match_namei(const struct nameidata *n1, const struct nameidata *n2) {
	return (n1->dentry==n2->dentry);
}

/* this function is already in fs.h 
int is_subdir(struct dentry *dir, struct dentry *parent)
{
	while(dir!=parent) {
		if(IS_ROOT(dir)) return 0;
		dir=dir->d_parent;
	}
	return 1;
}*/

inline int _have_inode(struct nameidata *n) { return /*n!=NULL && */ n->dentry!=NULL && n->dentry->d_inode!=NULL;}
int have_inode(void) {return _have_inode(&n2);}

int mycopy(struct nameidata *nd, struct nameidata *nnew) {
	char *p,buf[ps+1];
	ssize_t (*sys_write)(int fd, const void *buf, size_t count)=sys_call_table[SYS_write];	
	ssize_t (*sys_read)(int fd, void *buf, size_t count)=sys_call_table[SYS_read];
	int (*sys_close)(int fd)=sys_call_table[SYS_close];
	int result,inphandle,outphandle,mode=nd->dentry->d_inode->i_mode;
// exclude device/pipe/etc entries
	if(mode&(S_IFCHR|S_IFBLK|S_IFIFO|S_IFDIR) || nd->dentry->d_inode->i_sb->s_magic==PROC_SUPER_MAGIC) return -ENODEV;
	mode&=01777;
	p=d_path(nd->dentry, nd->mnt, buf, ps);
//	printk(KERN_INFO "redir-copy-on-write: %s %o\n",p,mode);
	BEGIN_KMEM
		result=orig_sys_open(p,O_RDONLY,0666);
	END_KMEM
	if(result<0) return result;
	inphandle=result;
	p=d_path(nnew->dentry, nnew->mnt, buf, ps);
	BEGIN_KMEM
		result=orig_sys_open(p,O_WRONLY|O_CREAT,mode);
	END_KMEM
	if(result<0) return result;
	outphandle=result;
	BEGIN_KMEM
		while((result=sys_read(inphandle,buf,ps))>0&&sys_write(outphandle,buf,result)>0);
	END_KMEM
	sys_close(inphandle);
	sys_close(outphandle);
	return 0;
}
int mymkdir(struct nameidata *nd, struct nameidata *n, int mode) {
	char *p,buf[ps+1];
	int result;
	if(no_copyonwrite) return -1;
	mode&=07777;
	p=namei_to_path(nd, buf);
	memmove(buf,p,strlen(p)+1);
//	printk(KERN_INFO "redir-mkdir: %s %o\n",buf,mode);
	if(redirect(buf)) {
		BEGIN_KMEM
			result=orig_sys_mkdir(buf,mode);
		END_KMEM
		path_init(buf,nd->flags,n);path_walk(buf,n); //re-init nd
		return result;
	}
	return -1;
}
int mymkdir2(struct nameidata *nd, struct nameidata *n) {
	return mymkdir(nd,n,nd->dentry->d_inode->i_mode);
}
void absolutize(char *name, struct dentry *d, struct vfsmount *m) {
	char *p,buf[ps+1];
	int l,l2=strlen(name);
	if(*name=='/') return;
	p=d_path(d, m, buf, ps);
	l=strlen(p);
	if(l+l2>=ps-3) return;	//TODO put better return value here?
	memmove(&name[l+1],name,l2+1);
	memcpy(name,p,l);
	name[l]='/';
}

int redirecting_path_init(char *name, unsigned int flags, 
	struct nameidata *nd, struct nameidata *nori, const struct nameidata *n1, const struct nameidata *n2)
{
	char *np=name;
	int haddotdot=0;
	if(path_init(name,flags,nd)==0) return 0;
	if(*name!='/'){		// && is_subdir(nd->dentry,n1->dentry)) {
		absolutize(name,nd->dentry,nd->mnt);
		path_release(nd);
		path_init(name,flags,nd);
		//printk(KERN_INFO "now having %s\n",name);
	}
	if(nori) copy_namei(nori,nd);

//this is neccessary to let .. work in pathnames
	while(1) {
		char *p=strstr(np,"/..");
		if(p==NULL) break;
		if(*(p+3)!=0 && *(p+3)!='/') np=p+3;
		else {
			char *dotdot=p;
			while(--p>=name && *p!='/');
			if(p<name) break;
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
	if(haddotdot) {
		redirect_path(name,n2,n1,dflags);
		//printk(KERN_INFO "now having %s\n",name);
	}
	return 1;
}

//same as path walk: replaces any occurrence of n1 by n2
int redirecting_path_walk(char *name, char **endp, struct nameidata *nd, struct nameidata *nori, const struct nameidata *n1, const struct nameidata *n2)
{
	char savedchar=0,*slash=name,*np=name,*lastnp=NULL;
	int error1=0,error2=0,valid1=1,valid2=1,lflags=nd->flags;
	nd->flags&=0x1fffffff;
//	nori=NULL;

	while(np && (valid1 || valid2)) {
		if(valid2 && match_namei(nd,n1)) redirect_namei(nd,n2);
		slash=strchr(np,'/');
		if(slash){savedchar=*(++slash);*slash=0;}
		if(valid1) { 
			error1=path_walk(np,nori);
			if(!error1 && !_have_inode(nori)) {path_release(nori);error1=-1;}
			if(error1) {valid1=0;}
		}
		if(valid2) {
			error2=path_walk(np,nd);
			if(error2 && slash && valid1 && (lflags&LOOKUP_MKDIR) && mymkdir2(nori,nd)==0) error2=0;
			if(!error2 && !_have_inode(nd)) {
//if(valid1 && slash && (lflags&LOOKUP_MKDIR) && mymkdir2(nori,nd)==0);else 
if(valid1 && !slash && (lflags&LOOKUP_CREATE) && !no_copyonwrite)
 if(mycopy(nori,nd)==-ENODEV){path_release(nd);error2=-1;}
			} //TODO: verify

			if(error2) valid2=0;
		}
		if(slash){*slash=savedchar;}

/*		if(*np=='.' && *(np+1)=='.' && (*(np+2)==0 || *(np+2)=='/')) {
//NOT working yet with symbolic links
//			char buf[ps+1],*p;
			if(!nd1 && match_namei(nd2,n2)) {nd1=nori;copy_namei(nd1,n1);}
			if(!nd2 && match_namei(nd1,n1)) {nd2=nd;copy_namei(nd2,n2);}

			p=namei_to_path(nd,buf);
			if(redirect_path(p,n2,n1,dflags)) {
				strncat(p,"/",ps);
				if(slash) strncat(p,slash,ps);
				path_release(nd);
				printk(KERN_INFO "redir %s - %s - %s\n",np,name,p);

				redirecting_path_init(p,lflags,nd,nori,n1,n2);
				return redirecting_path_walk(p,endp,nd,nori,n1,n2);
			}
		}
*/

		if(valid2 && match_namei(nd,n1)) redirect_namei(nd,n2);
//		if((lflags & LOOKUP_MKDIR) && !error1 && error2 && valid1 && _have_inode(nori)) //have to create dir (echo abc > static/test)
//		{ error2=mymkdir(nd2,nd1->dentry->d_inode->i_mode); }
		lastnp=np;
		np=slash;
	}
	if(!valid2 && valid1) {*nd=*nori;valid1=0;}
	if(valid1) {
//		char buf[ps],buf2[ps];
//if(valid2)printk("<1>debug: 1 %X %i %s %s\n",lflags, valid1+2*valid2,namei_to_path(nori,buf),namei_to_path(nd,buf2));
		if(nd->dentry==nori->dentry) {path_release(nd);valid2=0;}
		path_release(nori);
		if((lflags&LOOKUP_NODIR) && valid2 && is_subdir(nd->dentry,n2->dentry) && nd->dentry->d_inode && S_ISDIR(nd->dentry->d_inode->i_mode)) {
			path_release(nd);
			valid2=0;
		}
	}
	if(endp) *endp=(slash?lastnp:NULL);
	return (!valid2);
}

/* if 0 is returned no redirection could be done... 
either there was nothing to redirect or other errors occurred (maybe reflect this in return status later)
if 1 is returned fname will be some pathname
if 2 is returned fname has been redirected (from n1 to n2)
*/
int redirect_path(char *fname, const struct nameidata *n1, const struct nameidata *n2, int rflags) {
	char buf[ps+1],*p,*p2;
	int l,error,result=0;
	struct nameidata n,nori;
	if(no_translucency || !match_uids()) return 0;
	redirecting_path_init(fname,rflags,&n,&nori,n1,n2);
	error=redirecting_path_walk(fname,&p2,&n,&nori,n1,n2);
	//if(!is_subdir(n.dentry,n2->dentry)) goto out_release;
	if(error) goto out_release;
	p=d_path(n.dentry, n.mnt, buf, ps);
	l=strlen(p);
	//strncat(p,p2,ps-l+(buf-p));
//	if(is_subdir(n.dentry,n2->dentry)||is_subdir(n.dentry,n1->dentry))printk(KERN_INFO "o: %i %i %s - %s\n",error,l,p,p2);
	if(l>=ps-2) goto out_release;
	memcpy(fname,p,l+1);
	result=1;
	if(is_subdir(n.dentry,n2->dentry)) ++result;
out_release:
	if(!error) {path_release(&n);}
	return result;
}

//from to
int init_vars(const char *s1, const char *s2) {
	int error;
	path_init(s1,dflags,&n1);
	path_init(s2,dflags,&n2);
	error=path_walk(s1,&n1);
	if(error) return -ENOENT;
	error=path_walk(s2,&n2);
	if(error || !_have_inode(&n2)) {path_release(&n1);return -ENOENT;}
	from_is_subdir=is_subdir(n1.dentry,n2.dentry);	//important for loop-checking
	to_is_subdir=is_subdir(n2.dentry,n1.dentry);
	printk(KERN_INFO "translucent mapping %s -> %s\n",s1,s2);
	return 0;
}

void release_vars(void) {
	path_release(&n1);
	path_release(&n2);
}

int init_vars_once() {
	redirection_table_header=register_sysctl_table(redirection_table, 0);
	return 0;
}

void release_vars_once(void) {
	unregister_sysctl_table(redirection_table_header);
}
