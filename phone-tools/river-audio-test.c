/* Raw PCM diagnostic. Physical tests require a real backend; --plumbing opts
 * into testing the historical discard/synthetic endpoints explicitly. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include "river-audio-abi.h"

static int backend(int fd, int recording, int plumbing) {
    struct hakios_audio_status s = {0};
    if (ioctl(fd, HAKIOS_AUDIO_GET_STATUS, &s) < 0) {
        fprintf(stderr, "Audio backend status unavailable: %s. Physical audio is unverified.\n", strerror(errno));
        return plumbing ? 0 : -1;
    }
    if (s.version != HAKIOS_AUDIO_STATUS_VERSION) {
        fprintf(stderr, "Unsupported audio status version %u\n", s.version);
        return -1;
    }
    uint32_t flag = recording ? HAKIOS_AUDIO_CAPTURE_SYNTHETIC : HAKIOS_AUDIO_PLAYBACK_DISCARD;
    if (s.flags & flag) {
        fprintf(stderr, "%s; this is not a physical audio test.\n",
                recording ? "Microphone is synthetic" : "Playback PCM is discarded");
        return plumbing ? 0 : -1;
    }
    return 0;
}

static int write_all(int fd, const void *data, size_t size) {
    const unsigned char *p = data;
    while (size) {
        ssize_t n = write(fd, p, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { if (!n) errno = EIO; perror("write"); return -1; }
        p += n; size -= (size_t)n;
    }
    return 0;
}

static int number(const char *text, double min, double max, double *out) {
    char *end;
    errno = 0;
    double n = strtod(text, &end);
    if (errno || !*text || *end || !isfinite(n) || n < min || n > max) {
        fprintf(stderr, "Invalid number '%s' (range %.3g..%.3g)\n", text, min, max);
        return -1;
    }
    *out = n; return 0;
}

static int play_tone(double freq, double duration, double amp, int plumbing) {
    int fd = open(HAKIOS_AUDIO_DEVICE, O_WRONLY);
    if (fd < 0) { perror(HAKIOS_AUDIO_DEVICE); return 1; }
    int result = 1;
    if (backend(fd, 0, plumbing)) goto done;
    uint32_t total = (uint32_t)(duration * HAKIOS_AUDIO_RATE), frames = 0;
    int16_t buf[1024 * HAKIOS_AUDIO_CHANNELS];
    while (frames < total) {
        uint32_t n = total - frames;
        if (n > 1024) n = 1024;
        for (uint32_t i = 0; i < n; i++) {
            int16_t v = (int16_t)(sin(2 * M_PI * freq * (frames + i) / HAKIOS_AUDIO_RATE) * 32767 * amp);
            buf[i * 2] = buf[i * 2 + 1] = v;
        }
        if (write_all(fd, buf, n * HAKIOS_AUDIO_FRAME)) goto done;
        frames += n;
    }
    printf("Transferred %u output frames (%.3f s). Verify sound acoustically.\n", frames, duration);
    result = 0;
done:
    if (close(fd)) { perror("close audio"); result = 1; }
    return result;
}

static void le16(unsigned char *p, uint16_t n) { p[0] = n; p[1] = n >> 8; }
static void le32(unsigned char *p, uint32_t n) { le16(p, n); le16(p + 2, n >> 16); }
static int wav_header(int fd, uint32_t bytes) {
    unsigned char h[44] = {0};
    memcpy(h, "RIFF", 4); le32(h + 4, 36 + bytes); memcpy(h + 8, "WAVEfmt ", 8);
    le32(h + 16, 16); le16(h + 20, 1); le16(h + 22, 1);
    le32(h + 24, HAKIOS_AUDIO_RATE); le32(h + 28, HAKIOS_AUDIO_RATE * 2);
    le16(h + 32, 2); le16(h + 34, 16); memcpy(h + 36, "data", 4); le32(h + 40, bytes);
    if (lseek(fd, 0, SEEK_SET) < 0) { perror("seek WAV"); return -1; }
    return write_all(fd, h, sizeof(h));
}

static int record_mic(double duration, const char *path, int plumbing) {
    int fd = open(HAKIOS_MIC_DEVICE, O_RDONLY), out = -1, result = 1;
    if (fd < 0) { perror(HAKIOS_MIC_DEVICE); return 1; }
    if (backend(fd, 1, plumbing)) goto done;
    if (path) {
        out = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (out < 0) { perror(path); goto done; }
        if (wav_header(out, 0)) goto done;
    }
    uint32_t total = (uint32_t)(duration * HAKIOS_AUDIO_RATE), samples = 0;
    unsigned peak = 0;
    int16_t buf[512];
    while (samples < total) {
        uint32_t want = total - samples;
        if (want > 512) want = 512;
        ssize_t n = read(fd, buf, want * sizeof(*buf));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0 || n % 2) {
            if (n >= 0) errno = EIO;
            perror("read microphone (incomplete capture)"); goto done;
        }
        if (out >= 0 && write_all(out, buf, (size_t)n)) goto done;
        for (ssize_t i = 0; i < n / 2; i++) {
            unsigned v = (unsigned)abs((int)buf[i]); if (v > peak) peak = v;
        }
        samples += (uint32_t)n / 2;
    }
    if (out >= 0 && wav_header(out, samples * 2)) goto done;
    printf("Captured %u samples (%.3f s), peak %u/32768. Compare with an external acoustic signal.\n", samples, duration, peak);
    result = 0;
done:
    if (out >= 0) {
        if (close(out)) { perror("close WAV"); result = 1; }
        if (result) unlink(path); /* Do not leave a truncated test artifact. */
    }
    if (close(fd)) { perror("close microphone"); result = 1; }
    return result;
}

int main(int argc, char **argv) {
    int plumbing = argc > 1 && !strcmp(argv[argc - 1], "--plumbing");
    if (plumbing) argc--;
    if (argc < 2) goto usage;
    if (!strcmp(argv[1], "status") && argc == 2) {
        int fd = open(HAKIOS_AUDIO_DEVICE, O_RDONLY);
        if (fd < 0) { perror(HAKIOS_AUDIO_DEVICE); return 1; }
        struct hakios_audio_status s = {0};
        int rc = ioctl(fd, HAKIOS_AUDIO_GET_STATUS, &s);
        if (rc) perror("audio status");
        else printf("version=%u flags=%#x output_frames=%llu input_frames=%llu\n", s.version, s.flags,
                    (unsigned long long)s.output_frames, (unsigned long long)s.input_frames);
        close(fd); return rc ? 1 : 0;
    }
    if (!strcmp(argv[1], "play") && argc <= 5) {
        double freq = 440, dur = 2, amp = 0.05;
        if ((argc > 2 && number(argv[2], 20, 20000, &freq)) ||
            (argc > 3 && number(argv[3], 0.001, 60, &dur)) ||
            (argc > 4 && number(argv[4], 0, 1, &amp))) return 2;
        return play_tone(freq, dur, amp, plumbing);
    }
    if (!strcmp(argv[1], "record") && argc <= 4) {
        double dur = 3;
        if (argc > 2 && number(argv[2], 0.001, 60, &dur)) return 2;
        return record_mic(dur, argc > 3 ? argv[3] : NULL, plumbing);
    }
    if (!strcmp(argv[1], "vol") && argc <= 4) {
        double below = 0, muted = 0;
        if ((argc > 2 && number(argv[2], 0, 720, &below)) ||
            (argc > 3 && number(argv[3], 0, 1, &muted)) || below != floor(below) || muted != floor(muted)) return 2;
        int fd = open(HAKIOS_AUDIO_DEVICE, O_WRONLY);
        if (fd < 0) { perror(HAKIOS_AUDIO_DEVICE); return 1; }
        struct hakios_audio_volume v = { (uint32_t)below, (uint32_t)muted };
        int rc = ioctl(fd, HAKIOS_AUDIO_SET_VOLUME, &v);
        if (rc) perror("volume ioctl");
        close(fd); return rc ? 1 : 0;
    }
usage:
    fprintf(stderr, "Usage: %s status | play [Hz] [seconds] [amplitude] [--plumbing]\n"
                    "       %s record [seconds] [new.wav] [--plumbing]\n"
                    "       %s vol [dB_eighths 0..720] [muted 0|1]\n", argv[0], argv[0], argv[0]);
    return 2;
}
