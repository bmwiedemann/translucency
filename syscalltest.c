// gcc -I /usr/src/linux/include syscalltest.c -o syscalltest

#include <errno.h>
#include <syscall.h>
#define size_t int

_syscall3(size_t,listxattr, const char *,path, char *,list, size_t,size)
_syscall4(size_t,getxattr, const char *,path, char *,name, void *,value, size_t,size)

int main(int argc, char **argv) {
  char str[1000];
  int i;
  int result;
//  for(i=1; i<1000000; i++)
    result=getxattr("/tmp/fromdir/sub2/test","test",str,sizeof(str));
//    result=listxattr("/tmp/fromdir",str,sizeof(str));
  printf("%i %i\n", result,errno);
  return 0;
}
