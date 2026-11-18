#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef __APPLE__
typedef uintptr_t user_addr_t;
#endif
struct proc {int pid;};
static int proc_pid(struct proc *p){return p->pid;}
static void proc_name(int pid,char *out,size_t n){(void)pid;snprintf(out,n,"abi-test");}
static int in_error,out_error;static unsigned outs;static size_t out_bytes;
static int copyin(user_addr_t a,void *p,size_t n){if(in_error)return in_error;memcpy(p,(void*)a,n);return 0;}
static int copyout(const void *p,user_addr_t a,size_t n){outs++;out_bytes=n;if(out_error)return out_error;memcpy((void*)a,p,n);return 0;}
#include "token-actual.h"
int main(void){
 struct proc proc={123};unsigned char output[32];
 for(unsigned pid=0;pid<10;pid++){
  memset(output,0xa5,sizeof(output));uint64_t request[7]={0,3,0,0,(uintptr_t)(output+8),pid,0};
  outs=0;assert(!sweet_sandbox_syscall(&proc,5,(uintptr_t)request));assert(outs==1 && out_bytes==8 && !strcmp((char*)output+8,"invalid"));
  for(unsigned i=0;i<8;i++)assert(output[i]==0xa5);for(unsigned i=16;i<32;i++)assert(output[i]==0xa5);
 }
 uint64_t request[7]={0};outs=0;assert(sweet_sandbox_syscall(&proc,5,(uintptr_t)request)==EFAULT && !outs);
 request[4]=(uintptr_t)output;in_error=EFAULT;assert(sweet_sandbox_syscall(&proc,5,(uintptr_t)request)==EFAULT && !outs);in_error=0;
 out_error=EFAULT;assert(sweet_sandbox_syscall(&proc,5,(uintptr_t)request)==EFAULT);out_error=0;
 uint64_t handle=123,consume[3]={0,0,(uintptr_t)&handle};assert(!sweet_sandbox_syscall(&proc,6,(uintptr_t)consume) && !handle);
 assert(sweet_sandbox_syscall(&proc,4,0)==ENOENT && sweet_sandbox_syscall(&proc,0x43,0)==ENOENT);
 puts("PASS actual access-token ABI: zero/small/ordinary target PIDs accepted, exactly8bytes existing sentinel, canaries intact, null/inaccessible buffers fail, consume/container behavior unchanged");
}
