/* Read-only diagnostics; no direct register access from userland. */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
int main(void) {
    const char *names[] = { "debug.river_audio_prereq", "debug.river_audio_hw" };
    int result = 0;
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char data[4096] = {0}; size_t size = sizeof(data) - 1;
        if (sysctlbyname(names[i], data, &size, NULL, 0)) { perror(names[i]); result = 1; }
        else printf("%s:\n%s\n", names[i], data);
    }
    return result;
}
