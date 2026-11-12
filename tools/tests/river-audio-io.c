/* Host fault-injection tests for PCM transfer and artifact integrity. */
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int test_ioctl(int fd, unsigned long cmd, ...);
static int test_open(const char *path, int flags, ...);
static int test_close(int fd);
static ssize_t test_write(int fd, const void *p, size_t n);
static ssize_t test_read(int fd, void *p, size_t n);
#define ioctl test_ioctl
#define open test_open
#define close test_close
#define write test_write
#define read test_read
#define main audio_test_main
#include "../../phone-tools/river-audio-test.c"
#undef main
#undef ioctl
#undef open
#undef close
#undef write
#undef read
static int status_mode, io_mode, calls;
static size_t transferred;
static int test_ioctl(int fd, unsigned long cmd, ...) {
    (void)fd;
    va_list args; va_start(args, cmd); void *data = va_arg(args, void *); va_end(args);
    if (cmd != HAKIOS_AUDIO_GET_STATUS) return 0;
    if (status_mode == 0) { errno = ENOTTY; return -1; }
    struct hakios_audio_status *s = data;
    s->version = status_mode == 3 ? 99 : HAKIOS_AUDIO_STATUS_VERSION;
    s->flags = status_mode == 1 ? HAKIOS_AUDIO_CAPTURE_SYNTHETIC | HAKIOS_AUDIO_PLAYBACK_DISCARD : 0;
    return 0;
}
static int test_open(const char *p, int flags, ...) {
    if (!strcmp(p, HAKIOS_AUDIO_DEVICE) || !strcmp(p, HAKIOS_MIC_DEVICE)) return 9999;
    return open(p, flags, 0600);
}
static int test_close(int fd) { return fd == 9999 ? 0 : close(fd); }
static ssize_t test_write(int fd, const void *p, size_t n) {
    if (fd != 9999) return write(fd, p, n);
    calls++;
    if (io_mode == 1 && calls == 1) { errno = EINTR; return -1; }
    if (io_mode == 2) return 0;
    if (io_mode == 3) { errno = EIO; return -1; }
    if (n > 12) n = 12;
    transferred += n; return (ssize_t)n;
}
static ssize_t test_read(int fd, void *p, size_t n) {
    if (fd != 9999) return read(fd, p, n);
    calls++;
    if (io_mode == 1 && calls == 1) { errno = EINTR; return -1; }
    if (io_mode == 2 && calls > 1) return 0;
    if (io_mode == 3) { errno = EIO; return -1; }
    if (io_mode == 4) return 1;
    if (n > 14) n = 14;
    memset(p, 0, n); transferred += n; return (ssize_t)n;
}
int main(void) {
    double n;
    assert(number("nan", 0, 1, &n)); assert(number("inf", 0, 1, &n));
    assert(number("-1", 0, 1, &n)); assert(number("1junk", 0, 1, &n));
    assert(!number("0.05", 0, 1, &n));
    for (status_mode = 0; status_mode <= 3; status_mode++) {
        assert((backend(9999, 0, 0) == 0) == (status_mode == 2));
        assert((backend(9999, 1, 0) == 0) == (status_mode == 2));
        assert((backend(9999, 1, 1) == 0) == (status_mode != 3));
    }
    status_mode = 2;
    io_mode = 1; calls = 0; transferred = 0;
    assert(play_tone(440, 0.01, 0, 0) == 0); assert(transferred == 480 * 4);
    io_mode = 2; calls = 0; assert(play_tone(440, 0.01, 0, 0) == 1);
    io_mode = 3; calls = 0; assert(play_tone(440, 0.01, 0, 0) == 1);
    char path[] = "/tmp/river-audio-test-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0); close(fd);
    assert(record_mic(0.01, path, 0) == 1); /* O_EXCL must preserve existing file. */
    assert(access(path, F_OK) == 0); unlink(path);
    for (io_mode = 2; io_mode <= 4; io_mode++) {
        calls = 0; assert(record_mic(0.01, path, 0) == 1);
        assert(access(path, F_OK) < 0); /* No incomplete file survives an I/O error. */
    }
    io_mode = 1; calls = 0; transferred = 0;
    assert(record_mic(0.01, path, 0) == 0); assert(transferred == 480 * 2);
    fd = open(path, O_RDONLY); assert(fd >= 0);
    unsigned char h[44]; assert(read(fd, h, 44) == 44);
    assert(!memcmp(h, "RIFF", 4)); assert(!memcmp(h + 8, "WAVEfmt ", 8));
    assert((h[40] | h[41] << 8) == 960); assert(lseek(fd, 0, SEEK_END) == 1004);
    close(fd); unlink(path);
    puts("PASS: backend gating, EINTR, short transfers, EOF/errors, WAV integrity, no overwrite");
    return 0;
}
