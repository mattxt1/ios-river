/* --probe explicitly requests the one-shot identity read; default is passive. */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    int result = 0;
    if (argc == 2 && !strcmp(argv[1], "--probe")) {
        int request = 1;
        if (sysctlbyname("debug.river_codec_probe", NULL, NULL, &request, sizeof(request))) {
            perror("codec probe"); result = 1;
        }
    } else if (argc != 1) { fprintf(stderr, "Usage: %s [--probe]\n", argv[0]); return 2; }
    char note[512] = {0}; size_t n = sizeof(note) - 1;
    if (sysctlbyname("debug.river_codec_probe_result", note, &n, NULL, 0)) {
        perror("codec probe result"); return 1;
    }
    puts(note); return result;
}
