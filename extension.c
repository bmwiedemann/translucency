/*redirecting extension file
was automatically generated by script makeext.pl */

//modify as usual AND "make extdiff" for special (one time) adaptions

#include "base.h"

int (*orig_sys_open)(const char *pathname, int oflags, mode_t mode);
int (*orig_sys_creat)(const char *pathname, mode_t mode);
int (*orig_sys_truncate)(const char *path, off_t length);
int (*orig_sys_truncate64)(const char *path, loff_t length);
int (*orig_sys_stat)(const char *file_name, struct stat *buf);
int (*orig_sys_lstat)(const char *file_name, struct stat *buf);
int (*orig_sys_stat64)(const char *file_name, struct stat64 *buf);
int (*orig_sys_lstat64)(const char *file_name, struct stat64 *buf);
int (*orig_sys_access)(const char *pathname, int mode);
int (*orig_sys_readlink)(const char *path, char *buf, size_t bufsiz);
int (*orig_sys_rename)(const char *oldpath, const char *newpath);
int (*orig_sys_unlink)(const char *pathname);
int (*orig_sys_chdir)(const char *path);
int (*orig_sys_mkdir)(const char *pathname, mode_t mode);
int (*orig_sys_rmdir)(const char *pathname);
int (*orig_sys_chmod)(const char *path, mode_t mode);
int (*orig_sys_chown)(const char *path, uid_t owner, gid_t group);
int (*orig_sys_lchown)(const char *path, uid_t owner, gid_t group);
int (*orig_sys_chown32)(const char *path, uid_t owner, gid_t group);
int (*orig_sys_lchown32)(const char *path, uid_t owner, gid_t group);
int (*orig_sys_mknod)(const char *pathname, mode_t mode, dev_t dev);
int (*orig_sys_link)(const char *oldpath, const char *newpath);
int (*orig_sys_symlink)(const char *oldpath, const char *newpath);
int (*orig_sys_utime)(const char *filename, const struct utimbuf *buf);
int (*orig_sys_swapon)(const char *path, int swapflags);
int (*orig_sys_swapoff)(const char *path);
int (*orig_sys_getdents)(unsigned int fd, struct dirent *dirp, unsigned int count);
int (*orig_sys_getdents64)(unsigned int fd, struct dirent64 *dirp, unsigned int count);
int (*orig_sys_execve)(const char *filename, char *const argv [], char *const envp[]);

#define redir(a,b,c) b=sys_call_table[a];sys_call_table[a]=c
#define unredir(a,b,c) sys_call_table[a]=b

/* redirect syscalls (save old, install new) */
int init_module(void)
{
	int error;
	//char tobuf[ps]=defaultto, frombuf[ps]=defaultfrom;
	if(from) strncpy(frombuf,from,ps);
	if(to) strncpy(tobuf,to,ps);
	error=init_vars(frombuf,tobuf);
	if(error) return error;
	init_vars_once();
	redir(SYS_open, orig_sys_open, redirecting_sys_open);
	redir(SYS_creat, orig_sys_creat, redirecting_sys_creat);
	redir(SYS_truncate, orig_sys_truncate, redirecting_sys_truncate);
	redir(SYS_truncate64, orig_sys_truncate64, redirecting_sys_truncate64);
	redir(SYS_stat, orig_sys_stat, redirecting_sys_stat);
	redir(SYS_lstat, orig_sys_lstat, redirecting_sys_lstat);
	redir(SYS_stat64, orig_sys_stat64, redirecting_sys_stat64);
	redir(SYS_lstat64, orig_sys_lstat64, redirecting_sys_lstat64);
	redir(SYS_access, orig_sys_access, redirecting_sys_access);
	redir(SYS_readlink, orig_sys_readlink, redirecting_sys_readlink);
	redir(SYS_rename, orig_sys_rename, redirecting_sys_rename);
	redir(SYS_unlink, orig_sys_unlink, redirecting_sys_unlink);
	redir(SYS_chdir, orig_sys_chdir, redirecting_sys_chdir);
	redir(SYS_mkdir, orig_sys_mkdir, redirecting_sys_mkdir);
	redir(SYS_rmdir, orig_sys_rmdir, redirecting_sys_rmdir);
	redir(SYS_chmod, orig_sys_chmod, redirecting_sys_chmod);
	redir(SYS_chown, orig_sys_chown, redirecting_sys_chown);
	redir(SYS_lchown, orig_sys_lchown, redirecting_sys_lchown);
	redir(SYS_chown32, orig_sys_chown32, redirecting_sys_chown32);
	redir(SYS_lchown32, orig_sys_lchown32, redirecting_sys_lchown32);
	redir(SYS_mknod, orig_sys_mknod, redirecting_sys_mknod);
	redir(SYS_link, orig_sys_link, redirecting_sys_link);
	redir(SYS_symlink, orig_sys_symlink, redirecting_sys_symlink);
	redir(SYS_utime, orig_sys_utime, redirecting_sys_utime);
	redir(SYS_swapon, orig_sys_swapon, redirecting_sys_swapon);
	redir(SYS_swapoff, orig_sys_swapoff, redirecting_sys_swapoff);
	redir(SYS_getdents, orig_sys_getdents, redirecting_sys_getdents);
	redir(SYS_getdents64, orig_sys_getdents64, redirecting_sys_getdents64);
	redir(SYS_execve, orig_sys_execve, redirecting_sys_execve);
	return 0;
}

/* unredirect syscalls
 this is problematic when several syscall-redirecting modules are loaded
 and not unloaded in reverse order!!!
*/
void cleanup_module(void)
{
	release_vars_once();
	release_vars();
	unredir(SYS_open, orig_sys_open, redirecting_sys_open);
	unredir(SYS_creat, orig_sys_creat, redirecting_sys_creat);
	unredir(SYS_truncate, orig_sys_truncate, redirecting_sys_truncate);
	unredir(SYS_truncate64, orig_sys_truncate64, redirecting_sys_truncate64);
	unredir(SYS_stat, orig_sys_stat, redirecting_sys_stat);
	unredir(SYS_lstat, orig_sys_lstat, redirecting_sys_lstat);
	unredir(SYS_stat64, orig_sys_stat64, redirecting_sys_stat64);
	unredir(SYS_lstat64, orig_sys_lstat64, redirecting_sys_lstat64);
	unredir(SYS_access, orig_sys_access, redirecting_sys_access);
	unredir(SYS_readlink, orig_sys_readlink, redirecting_sys_readlink);
	unredir(SYS_rename, orig_sys_rename, redirecting_sys_rename);
	unredir(SYS_unlink, orig_sys_unlink, redirecting_sys_unlink);
	unredir(SYS_chdir, orig_sys_chdir, redirecting_sys_chdir);
	unredir(SYS_mkdir, orig_sys_mkdir, redirecting_sys_mkdir);
	unredir(SYS_rmdir, orig_sys_rmdir, redirecting_sys_rmdir);
	unredir(SYS_chmod, orig_sys_chmod, redirecting_sys_chmod);
	unredir(SYS_chown, orig_sys_chown, redirecting_sys_chown);
	unredir(SYS_lchown, orig_sys_lchown, redirecting_sys_lchown);
	unredir(SYS_chown32, orig_sys_chown32, redirecting_sys_chown32);
	unredir(SYS_lchown32, orig_sys_lchown32, redirecting_sys_lchown32);
	unredir(SYS_mknod, orig_sys_mknod, redirecting_sys_mknod);
	unredir(SYS_link, orig_sys_link, redirecting_sys_link);
	unredir(SYS_symlink, orig_sys_symlink, redirecting_sys_symlink);
	unredir(SYS_utime, orig_sys_utime, redirecting_sys_utime);
	unredir(SYS_swapon, orig_sys_swapon, redirecting_sys_swapon);
	unredir(SYS_swapoff, orig_sys_swapoff, redirecting_sys_swapoff);
	unredir(SYS_getdents, orig_sys_getdents, redirecting_sys_getdents);
	unredir(SYS_getdents64, orig_sys_getdents64, redirecting_sys_getdents64);
	unredir(SYS_execve, orig_sys_execve, redirecting_sys_execve);
}


int redirecting_sys_creat(const char *pathname, mode_t mode)
{
	char local0[ps];
	if(strncpy_from_user(local0, pathname, ps)<0) return -EFAULT;
	if(dredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_creat(local0, mode);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_creat(pathname, mode);
}

int redirecting_sys_truncate(const char *path, off_t length)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(wredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_truncate(local0, length);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_truncate(path, length);
}

int redirecting_sys_truncate64(const char *path, loff_t length)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(wredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_truncate64(local0, length);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_truncate64(path, length);
}

int redirecting_sys_stat(const char *file_name, struct stat *buf)
{
	char local0[ps];
	struct stat local1;
	if(strncpy_from_user(local0, file_name, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_stat(local0, &local1);
		END_KMEM
		if(copy_to_user(buf, &local1, sizeof(local1))){return -EFAULT;}
		if(result!=-ENOENT) return result;
	}
	return orig_sys_stat(file_name, buf);
}

int redirecting_sys_lstat(const char *file_name, struct stat *buf)
{
	char local0[ps];
	struct stat local1;
	if(strncpy_from_user(local0, file_name, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_lstat(local0, &local1);
		END_KMEM
		if(copy_to_user(buf, &local1, sizeof(local1))){return -EFAULT;}
		if(result!=-ENOENT) return result;
	}
	return orig_sys_lstat(file_name, buf);
}

int redirecting_sys_stat64(const char *file_name, struct stat64 *buf)
{
	char local0[ps];
	struct stat64 local1;
	if(strncpy_from_user(local0, file_name, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_stat64(local0, &local1);
		END_KMEM
		if(copy_to_user(buf, &local1, sizeof(local1))){return -EFAULT;}
		if(result!=-ENOENT) return result;
	}
	return orig_sys_stat64(file_name, buf);
}

int redirecting_sys_lstat64(const char *file_name, struct stat64 *buf)
{
	char local0[ps];
	struct stat64 local1;
	if(strncpy_from_user(local0, file_name, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_lstat64(local0, &local1);
		END_KMEM
		if(copy_to_user(buf, &local1, sizeof(local1))){return -EFAULT;}
		if(result!=-ENOENT) return result;
	}
	return orig_sys_lstat64(file_name, buf);
}

int redirecting_sys_access(const char *pathname, int mode)
{
	char local0[ps];
	if(strncpy_from_user(local0, pathname, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_access(local0, mode);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_access(pathname, mode);
}

int redirecting_sys_readlink(const char *path, char *buf, size_t bufsiz)
{
	char local0[ps];
	char local1[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_readlink(local0, local1, bufsiz);
		END_KMEM
		if(result>=0 && result<bufsiz){
			//if(unredirect(local1)) result=strlen(local1)+1;
			if(copy_to_user(buf, local1, result+1)) return -EFAULT;
		}
		if(result!=-ENOENT) return result;
	}
	return orig_sys_readlink(path, buf, bufsiz);
}

int redirecting_sys_rename(const char *oldpath, const char *newpath)
{
	char local0[ps];
	char local1[ps];
	if(strncpy_from_user(local0, oldpath, ps)<0) return -EFAULT;
	if(dredirect(local0)) {
		int result;	if(strncpy_from_user(local1, newpath, ps)<0) return -EFAULT; redirect(local1);
		BEGIN_KMEM
			result = orig_sys_rename(local0, local1);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_rename(oldpath, newpath);
}

int redirecting_sys_unlink(const char *pathname)
{
	char local0[ps];
	if(strncpy_from_user(local0, pathname, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_unlink(local0);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_unlink(pathname);
}

int redirecting_sys_mkdir(const char *pathname, mode_t mode)
{
	char local0[ps];
	if(strncpy_from_user(local0, pathname, ps)<0) return -EFAULT;
	if(dredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_mkdir(local0, mode);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_mkdir(pathname, mode);
}

int redirecting_sys_rmdir(const char *pathname)
{
	char local0[ps];
	if(strncpy_from_user(local0, pathname, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_rmdir(local0);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_rmdir(pathname);
}

int redirecting_sys_chmod(const char *path, mode_t mode)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(wredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_chmod(local0, mode);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_chmod(path, mode);
}

int redirecting_sys_chown(const char *path, uid_t owner, gid_t group)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(wredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_chown(local0, owner, group);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_chown(path, owner, group);
}

int redirecting_sys_lchown(const char *path, uid_t owner, gid_t group)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(wredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_lchown(local0, owner, group);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_lchown(path, owner, group);
}

int redirecting_sys_chown32(const char *path, uid_t owner, gid_t group)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(wredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_chown32(local0, owner, group);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_chown32(path, owner, group);
}

int redirecting_sys_lchown32(const char *path, uid_t owner, gid_t group)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(wredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_lchown32(local0, owner, group);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_lchown32(path, owner, group);
}

int redirecting_sys_mknod(const char *pathname, mode_t mode, dev_t dev)
{
	char local0[ps];
	if(strncpy_from_user(local0, pathname, ps)<0) return -EFAULT;
	if(dredirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_mknod(local0, mode, dev);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_mknod(pathname, mode, dev);
}

int redirecting_sys_link(const char *oldpath, const char *newpath)
{
	char local0[ps];
	char local1[ps];
	if(strncpy_from_user(local0, oldpath, ps)<0) return -EFAULT;
	if(dredirect(local0)) {
		int result;	if(strncpy_from_user(local1, newpath, ps)<0) return -EFAULT; redirect(local1);
		BEGIN_KMEM
			result = orig_sys_link(local0, local1);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_link(oldpath, newpath);
}

int redirecting_sys_swapon(const char *path, int swapflags)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_swapon(local0, swapflags);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_swapon(path, swapflags);
}

int redirecting_sys_swapoff(const char *path)
{
	char local0[ps];
	if(strncpy_from_user(local0, path, ps)<0) return -EFAULT;
	if(redirect(local0)) {
		int result;
		BEGIN_KMEM
			result = orig_sys_swapoff(local0);
		END_KMEM
		if(result!=-ENOENT) return result;
	}
	return orig_sys_swapoff(path);
}

/* EOF */
