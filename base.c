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

/* There are up to REDIRS redirections possible */
struct translucent redirs[REDIRS];
char translucent_delimiter[16]=" -> ";	// set to "" for binfmt_misc syntax
int translucent_uid   = ANYUID;
int translucent_gid   = ANYUID;
int translucent_fs    = 0;
int translucent_flags = 0;
int translucent_cnt   = 0;

static inline int match_uids(void)
{
   struct task_struct *p = current;
   return (( translucent_uid == ANYUID || (unsigned)translucent_uid == p->euid) &&
	   ( translucent_gid == ANYUID || (unsigned)translucent_gid == p->egid));
}

static inline int match_fs(const struct nameidata *n, const struct translucent *t)
{
	int magic;
	//TODO: remove debug entries if never seen in syslog for months
	if(!n) {printk("DEBUG0a");return 0;}
	if(!n->dentry) {printk("DEBUG1a");return 0;}
	if(!n->dentry->d_inode) {printk("DEBUG2a");return 0;}
	if(!n->dentry->d_inode->i_sb) {printk("DEBUG3a");return 0;}
	if(!t) {printk("DEBUG0b");return 0;}
	if(!t->n[0].dentry) {printk("DEBUG1b");return 0;}
	if(!t->n[0].dentry->d_inode) {printk("DEBUG2b");return 0;}
	if(!t->n[0].dentry->d_inode->i_sb) {printk("DEBUG3b");return 0;}
	magic=n->dentry->d_inode->i_sb->s_magic;
	if((unsigned)magic==t->n[0].dentry->d_inode->i_sb->s_magic ||
		(translucent_fs>0 && magic==translucent_fs) ||
		(translucent_fs<0 && magic!=-translucent_fs)) return 1;
	return 0;
}

void redirect_namei(struct nameidata *dest, const struct nameidata *src) {
	mntget(src->mnt);  dget(src->dentry);
	mntput(dest->mnt); dput(dest->dentry);
	*dest = *src;
}

static inline void copy_namei(struct nameidata *dest, const struct nameidata *src) {
	mntget(src->mnt);  dget(src->dentry);
	*dest = *src;
}

static inline int match_namei(const struct nameidata *n1, const struct nameidata *n2) {
	return (n1->dentry == n2->dentry);
}

inline int have_inode(struct nameidata *n) { 
	return /*n!=NULL && */ n->dentry!=NULL && n->dentry->d_inode!=NULL;
}

inline int is_special(struct nameidata *n) {
	return have_inode(n) && (!S_ISREG(n->dentry->d_inode->i_mode) 
		|| n->dentry->d_inode->i_sb->s_magic==PROC_SUPER_MAGIC);
}

static inline int translucent_is_deleted(struct nameidata *n) {
  return n && have_inode(n) && n->dentry->d_inode->i_size==0 &&
	((n->dentry->d_inode->i_mode&(S_IALLUGO|S_IFMT))==(S_IFREG|01001));
}

int translucent_copy(struct nameidata *nd, struct nameidata *nnew, int lookup_flags) {
	char *p,buf[REDIR_BUFSIZE+1];
	ssize_t (*sys_write)(int fd, const void *buf, size_t count)=sys_call_table[__NR_write];	
	ssize_t (*sys_read)(int fd, void *buf, size_t count)=sys_call_table[__NR_read];
	int (*sys_close)(int fd)=sys_call_table[__NR_close];
	int result,inphandle,outphandle,mode=nd->dentry->d_inode->i_mode;

// exclude device/pipe/socket/dir and proc entries from COW
	if(is_special(nd)) return -ENODEV;

	mode &= S_IRWXUGO;
	p = d_path(nd->dentry, nd->mnt, buf, REDIR_BUFSIZE);

//	printk(KERN_DEBUG SYSLOGID ": copy-on-write %s %o\n",p,mode);

	BEGIN_KMEM
		result=orig_sys_open(p,O_RDONLY,0666);
	END_KMEM
	if(result<0) return result;
	inphandle=result;

	p=d_path(nnew->dentry, nnew->mnt, buf, REDIR_BUFSIZE);
	BEGIN_KMEM
		result=orig_sys_open(p,O_WRONLY|O_CREAT,mode);
	END_KMEM
	if(result<0) goto out_close;
	outphandle=result;

	if(!(lookup_flags&LOOKUP_TRUNCATE)) {
		BEGIN_KMEM
			while((result=sys_read(inphandle,buf,REDIR_BUFSIZE))>0&&sys_write(outphandle,buf,result)>0);
		END_KMEM
	} else result=0;
	sys_close(outphandle);
out_close:
	sys_close(inphandle);
	return result;
}

int translucent_mkdir(struct nameidata *nd, struct nameidata *n, int mode, struct translucent *t) {
	char *p,buf[REDIR_BUFSIZE+1];
	int result, umask=current->fs->umask;
	if ((translucent_flags&no_copyonwrite)) return -1;
	mode &= 07777;
	p = namei_to_path(nd, buf);
	memmove(buf,p,strlen(p)+1);
//	printk(KERN_DEBUG SYSLOGID ": mkdir %s %.4o\n",buf,mode);
	if(redirectt(t,buf)) {
		current->fs->umask = 0;
		BEGIN_KMEM
			result=orig_sys_mkdir(buf,mode);
		END_KMEM
		current->fs->umask = umask;
		if(result==0) { //re-init n
			path_init(buf,nd->flags,n);
			result=path_walk(buf,n);
		}
		return result;
	}
	return -1;
}

static inline int mymkdir2(struct nameidata *nd, struct nameidata *n, struct translucent *t) {
	return translucent_mkdir(nd,n,nd->dentry->d_inode->i_mode,t);
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
		       struct nameidata *n, struct translucent *t)
{
	char *np=name;
	int haddotdot=0,i;
	if (path_init(name,flags,n)==0) return 0;
	if (*name!='/'){		// && is_subdir(nd->dentry,n1->dentry)) {
		absolutize(name,n->dentry,n->mnt);
		path_release(n);
		path_init(name,flags,n);
		//printk(KERN_INFO "now having %s\n",name);
	}
	for(i=1; i<t->layers; ++i) copy_namei(&n[i],n);

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
		//TODO: is that "reverse redirection" needed? redirect_path (name,t,n2,n1,dflags);
		//printk(KERN_INFO "now having %s\n",name);
	}
	return 1;
}

// returns highest index of valid layer and -1 if nil
static inline int any_valid(int layers, char *valid)
{
	int i;
	for(i=layers-1; i>=0; --i) if(valid[i]) break;
	return i;
}

#define redirect_all_namei \
	for(i=1; i<t->layers; ++i) if(valid[i] && match_namei(&n[i],&t->n[0])) \
	{ redirect_namei(&n[i],&t->n[i]); something_redirected=1; }

//same as path walk: replaces any occurrence of n1 by n2
int redirect_path_walk(char *name, char **endp, 
		       struct nameidata *n, struct translucent *t) 
{
	char savedchar=0,*slash=name,*np=name,*lastnp=NULL,something_redirected=0,valid[MAX_LAYERS];
	int lflags=n->flags,error=0,i,j;
	n->flags &= LOOKUP_TRANSLUCENCY_MASK;
	memset(valid, 1, sizeof(valid));

	while (np && any_valid(t->layers,valid)>=0) {
		redirect_all_namei;
		slash = strchr(np,'/');
		if (slash) { savedchar = *(++slash); *slash=0; }
		for(i=0; i<t->layers; ++i)
		 if(valid[i]) {
			error = path_walk(np,&n[i]);
			if(i == t->layers-1) {
			 if(error && slash && (lflags & LOOKUP_MKDIR)) {
				int j=any_valid(t->layers-1,valid);
				if (j>=0 && mymkdir2(&n[j],&n[i],t)==0) error = 0;
			 }
			} 
			else if (!error && !have_inode(&n[i])) { path_release(&n[i]); error = -1; }
			if (error) valid[i] = 0;
			if(something_redirected && i==0 && valid[i] && !match_fs(&n[i],t))
			  for(i=0; i<t->layers; ++i) if(valid[i]) {
				path_release(&n[i]);
				valid[i]=0;
			  }
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
		redirect_all_namei;
		lastnp=np;
		np=slash;
	}	// end of main redirect/walking loop

	if(!slash && valid[t->layers-1] && !have_inode(&n[t->layers-1])) {
		int mode, umask=current->fs->umask;
		for(j=t->layers-2; j>=0; --j) if(valid[j] && have_inode(&n[j])) break;
		error=0;
		if (j>=0) {
			current->fs->umask=0;
			// prevent COW and overlaying of files in topmost dir
			if(is_subdir(n[j].dentry, t->n[t->layers-1].dentry)) error = -1;
			else {
		 mode=n[j].dentry->d_inode->i_mode;
		 // this does COW
		 if(is_special(&n[j])) {
			if((lflags&LOOKUP_NOSPECIAL)) error = -1; // no mknod on echo > /dev/null
			else if((lflags&LOOKUP_CREATE) && (S_ISBLK(mode) || S_ISCHR(mode)) && !(translucent_flags&no_copyonwrite)) {
				// this is inlined quasi "mymknod"
				char buf[REDIR_BUFSIZE],*p=namei_to_path(&n[t->layers-1],buf);
				memmove(buf,p,strlen(p)+1);p=buf;
//				printk(KERN_DEBUG SYSLOGID ": mknod %s %.6o %X\n",p,mode,n[j].dentry->d_inode->i_rdev);
				BEGIN_KMEM
					error = orig_sys_mknod(p,mode,n[j].dentry->d_inode->i_rdev);
				END_KMEM
			}
		 } else if((lflags&LOOKUP_CREATE) && !(translucent_flags&no_copyonwrite)) {
//			char buf[REDIR_BUFSIZE];printk(KERN_DEBUG SYSLOGID ": mycopy %s %.6o\n",namei_to_path(nd,buf),mode);
			error = translucent_copy(&n[j],&n[t->layers-1],lflags);
		 }
			}
			current->fs->umask = umask;
			if(error) { path_release(&n[t->layers-1]); valid[t->layers-1]=0; }
		}
	}

	// now free unredirected paths
	i=any_valid(t->layers,valid);
	while (i>0 && n[i].dentry==n[0].dentry) {
		path_release(&n[i]);
		valid[i]=0;
		i=any_valid(i,valid);
	}
	// return dir in lowest layer to open(dir) call
	if(i>0 && (lflags&LOOKUP_NODIR)) {
		for(j=0; j<=i; ++j) if(valid[j] && have_inode(&n[j]) && S_ISDIR(n[j].dentry->d_inode->i_mode)) break;
		for(; i>j; --i) if(valid[i]) {
			path_release(&n[i]);
			valid[i]=0;
		}
	}
	// return index of uppermost layer with valid entry
	for(j=i-1; j>=0; --j) if(valid[j]) path_release(&n[j]); //valid[j]=0; }
	if (endp) *endp=(slash?lastnp:NULL);
	if(!something_redirected && i>=0) {path_release(&n[i]); return (-1);}
	return (i);
}

/* if 0 is returned no redirection could be done... 
   either there was nothing to redirect or other errors occurred (maybe reflect this in return status later)
   if 1 is returned fname will be some pathname
   if 2 is returned fname has been redirected (from n1 to n2)
*/
int redirect_path(char *fname, struct translucent *t, int rflags) 
{
	char *p,*p2;
	int i,error=1,result=0,top=-1;
	struct nameidata n[MAX_LAYERS];
	if ((translucent_flags & no_translucency) || !match_uids()) return 0;
	if ((translucent_flags & do_downcase)) for (i=strlen(fname)-1; i>=0; --i) 
		if (fname[i]>='A' && fname[i]<='Z') fname[i]|=32;
	if (t == NULL) {
		for (i=0; i<REDIRS; ++i) {
			if (is_valid(&redirs[i])) {
				t = &redirs[i];
				redirect_path_init(fname,rflags,n,t);
				top = redirect_path_walk(fname,&p2,n,t);
				if (top>0) break;
			}
		}
	} else {
		redirect_path_init(fname,rflags,n,t);
		top = redirect_path_walk(fname,&p2,n,t);
	}
	error=top<0;
	if (error) goto out_release;
	p = d_path(n[top].dentry, n[top].mnt, fname, REDIR_BUFSIZE);
	memmove(fname,p,strlen(p)+1);
	result=1;
	for(i=1; i<t->layers; ++i) if(is_subdir(n[top].dentry, t->n[i].dentry)) result=2;
//	if(result==2) printk(KERN_DEBUG "o: %i %i %s - %s\n",error,l,p,p2);
	if(translucent_is_deleted(&n[top]) && top>0 && !(rflags&LOOKUP_CREATES)) {
		//printk(KERN_INFO "translucency: deletion of %s detected\n",fname);
		result=-ENOENT;
	}
out_release:
	if (!error) path_release(&n[top]);
	return result;
}

static void cleanup_translucent(struct translucent *t) 
{
	if (is_valid(t)) {
		path_release(&t->n[0]);
		path_release(&t->n[1]);
		t->valid = 0;
		memset(t->b,    0, sizeof(t->b));
		MOD_DEC_USE_COUNT; --translucent_cnt;
	}
}

static int init_translucent(struct translucent *t) 
{
	char buf[REDIR_BUFSIZE], *p=buf,*endp=buf, del=0, delsize=1,
		delimiter[sizeof(translucent_delimiter)];
	int error, i,n=0,l=strlen(p)-1;
	if(p[l]=='\n') p[l]=0;		// strip LF
	if (t->b[0] == 0) {
		printk(KERN_INFO SYSLOGID ": mapping %d removed\n", (int)t->index);
		return 0;
	}
	strncpy(buf, t->b, sizeof(buf));
	strcpy(delimiter, translucent_delimiter);
	if(!delimiter[0]) del=*(p++);	// for binfmt_misc syntax
	else delsize=strlen(delimiter);
	while(endp && *p) {
		if(delimiter[0]) endp=strstr(p,delimiter);
		else endp=strchr(p,del);
		if(endp) {
			*endp=0;
		}
		if(*p) {
			// initialization of layer n
			if(n>=MAX_LAYERS) {
				printk(KERN_ERR SYSLOGID ": error MAX_LAYERS=%i\n",MAX_LAYERS);
				for(i=0; i<n; ++i) path_release(&t->n[i]);
				return -EINVAL;
			}
			path_init(p, dflags, &t->n[n]);
			error = path_walk(p, &t->n[n]);
			if(!have_inode(&t->n[n])) { error=-1; path_release(&t->n[n]); }
			if (error) {
				for(i=0; i<n; ++i) path_release(&t->n[i]);
				printk(KERN_ERR SYSLOGID ": %s not found\n", p);
				return -ENOENT;
			}
			++n;
		}
		p=endp+delsize;
	}
	t->layers=n;
	printk(KERN_INFO SYSLOGID ": mapping %d established ", t->index);
	for(i=0; i<t->layers; ++i) {
		p=namei_to_path(&t->n[i],t->b);
		printk("%s%s",p,i==t->layers-1?"\n":" -> ");
	}
	t->valid = valid_translucency;
	MOD_INC_USE_COUNT; ++translucent_cnt;
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

struct ctl_table redirection_dir_table[REDIRS+CTL_TABLE_STATIC] = {
	{CTL_TABLE_BASE+1, "uid",   &translucent_uid,   sizeof(translucent_uid),   0644, NULL, &proc_dointvec, NULL, NULL, NULL, NULL},
	{CTL_TABLE_BASE+2, "gid",   &translucent_gid,   sizeof(translucent_gid),   0644, NULL, &proc_dointvec, NULL, NULL, NULL, NULL},
	{CTL_TABLE_BASE+3, "flags", &translucent_flags, sizeof(translucent_flags), 0644, NULL, &proc_dointvec, NULL, NULL, NULL, NULL},
	{CTL_TABLE_BASE+6, "fs",    &translucent_fs,    sizeof(translucent_fs),    0644, NULL, &proc_dointvec, NULL, NULL, NULL, NULL},
	{CTL_TABLE_BASE+4, "used",  &translucent_cnt,   sizeof(translucent_cnt),   0444, NULL, &proc_doint_ro, NULL, NULL, NULL, NULL},
	{CTL_TABLE_BASE+5, "delimiter", &translucent_delimiter, sizeof(translucent_flags), 0644, NULL, &proc_dostring, NULL, NULL, NULL, NULL},
};
struct ctl_table_header *redirection_table_header;
struct ctl_table redirection_table[] = {
	{CTL_TABLE_BASE, "translucency", NULL, 0, 0555, redirection_dir_table, NULL, NULL, NULL, NULL, NULL},
	{0}
};

// this *5 means four digits per entry, allowing at least REDIRS==9999
char redirection_dir_table_name_buffer[REDIRS*5];

int __init translucent_init_module(void) 
{
	int i;
	char *buffer_allocated=redirection_dir_table_name_buffer;
	memset(&redirs, 0, sizeof(redirs));
	for (i=0; i<REDIRS; ++i) {
		struct ctl_table new={CTL_ENTRY_BASE+i,buffer_allocated,&redirs[i].b,REDIR_BUFSIZE-1,0644,NULL,&redir_handler,NULL,NULL,NULL,NULL};
		sprintf(buffer_allocated,"%d",i);
		buffer_allocated+=5;
		redirection_dir_table[CTL_TABLE_STATIC+i-1]=new;
		redirs[i].index=i;
	}
	//assert(redirection_dir_table[REDIRS+CTL_TABLE_STATIC-1]=={0})
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
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

module_init(translucent_init_module);
module_exit(translucent_cleanup_module);
