/* Bounded read-only QG survey. Each line contains sequential, not atomic,
 * observations. Use a new path on a persistent filesystem if the log must
 * survive reboot; the caller must verify that filesystem's behavior first. */
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
 if (argc != 4) {
  fprintf(stderr, "usage: river-qg-log NEW_PATH SAMPLES INTERVAL_SECONDS\n"); return 2;
 }
 char *end;
 long count = strtol(argv[2], &end, 10);
 if (*end || count < 1 || count > 3600) return 2;
 long interval = strtol(argv[3], &end, 10);
 if (*end || interval < 1 || interval > 3600) return 2;
 int fd = open(argv[1], O_WRONLY | O_CREAT | O_EXCL, 0600);
 if (fd < 0) { perror("open"); return 1; }
 FILE *f = fdopen(fd, "w");
 if (!f) { perror("fdopen"); close(fd); return 1; }
 const char *names[] = {"debug.river_qg_sample", "debug.river_qg_compare",
     "debug.river_charge_sample", "debug.river_spmi_gate"};
 for (long n = 0; n < count; n++) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL)) { perror("gettimeofday"); fclose(f); return 1; }
  fprintf(f, "sample=%ld time=%lld.%06d", n, (long long)tv.tv_sec, (int)tv.tv_usec);
  for (unsigned j = 0; j < sizeof(names) / sizeof(names[0]); j++) {
   char buf[512] = {0}; size_t len = sizeof(buf);
   int rc = sysctlbyname(names[j], buf, &len, NULL, 0), saved_errno = errno;
   if (rc) fprintf(f, " | %s errno=%d", names[j], saved_errno);
   else fprintf(f, " | %s %s", names[j], buf);
  }
  fputc('\n', f);
  if (fflush(f) || fsync(fd)) { perror("log write"); fclose(f); return 1; }
  if (n + 1 < count) {
   unsigned left = (unsigned)interval;
   while (left) left = sleep(left);
  }
 }
 if (fclose(f)) { perror("close"); return 1; }
 return 0;
}
