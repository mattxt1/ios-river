/* Bounded read-only copy of the retained kernel message buffer. Does not
 * consume /dev/klog, change buffer size or bypass the kernel's root/MAC checks.
 * Output can contain private device/network details; keep captures out of Git. */
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sysctl.h>

extern int proc_kmsgbuf(void *, uint32_t);

int main(void)
{
 static char buffer[1024 * 1024];
 int capacity=0; size_t size=sizeof(capacity);
 if (!sysctlbyname("kern.msgbuf",&capacity,&size,NULL,0))
  fprintf(stderr,"kernel_message_buffer_capacity=%d\n",capacity);
 int count = proc_kmsgbuf(buffer, sizeof(buffer));
 if (count < 0) { perror("proc_kmsgbuf"); return 1; }
 if ((unsigned)count > sizeof(buffer)) {
  fputs("Invalid kernel message length\n", stderr); return 1;
 }
 size_t done = 0;
 while (done < (size_t)count) {
  ssize_t n = write(STDOUT_FILENO, buffer + done, (size_t)count - done);
  if (n < 0 && errno == EINTR) continue;
  if (n <= 0) { if (!n) errno = EIO; perror("write"); return 1; }
  done += (size_t)n;
 }
 return 0;
}
