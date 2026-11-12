#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
 int rc = 0, request = 0;
 if (argc == 2 && !strcmp(argv[1], "--snapshot")) request = 1;
 else if (argc == 2 && !strcmp(argv[1], "--test")) request = 2;
 else if (argc != 1) { fprintf(stderr, "Usage: %s [--snapshot|--test]\n", argv[0]); return 2; }
 if (request && sysctlbyname("debug.river_codec_clock", NULL, NULL, &request, sizeof(request))) {
  perror("codec clock"); rc = 1;
 }
 char note[2048] = {0}; size_t n = sizeof(note) - 1;
 if (sysctlbyname("debug.river_codec_clock_result", note, &n, NULL, 0)) { perror("codec clock result"); return 1; }
 puts(note); return rc;
}
