#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <stdint.h>

#define HAKIOS_AUDIO_DEVICE    "/dev/hakios-audio"
#define HAKIOS_MIC_DEVICE      "/dev/hakios-mic"
#define HAKIOS_AUDIO_RATE      48000
#define HAKIOS_AUDIO_CHANNELS  2

struct hakios_audio_volume {
    uint32_t below; /* eighths of a dB */
    uint32_t muted;
};
#define HAKIOS_AUDIO_SET_VOLUME _IOW('r', 1, struct hakios_audio_volume)

static int play_tone(float freq, float duration, float amp) {
    int fd = open(HAKIOS_AUDIO_DEVICE, O_WRONLY);
    if (fd < 0) {
        perror("open " HAKIOS_AUDIO_DEVICE);
        return 1;
    }
    printf("Playing %.1f Hz tone for %.1f s on %s (amp %.2f)...\n",
           freq, duration, HAKIOS_AUDIO_DEVICE, amp);

    int total_frames = (int)(duration * HAKIOS_AUDIO_RATE);
    int chunk_frames = 1024;
    int16_t *buf = malloc(chunk_frames * HAKIOS_AUDIO_CHANNELS * sizeof(int16_t));

    int frames_left = total_frames;
    int current_frame = 0;
    while (frames_left > 0) {
        int n = frames_left < chunk_frames ? frames_left : chunk_frames;
        for (int i = 0; i < n; i++) {
            double t = (double)(current_frame + i) / HAKIOS_AUDIO_RATE;
            int16_t val = (int16_t)(sin(2.0 * M_PI * freq * t) * 32767.0 * amp);
            buf[i * 2 + 0] = val; // Left
            buf[i * 2 + 1] = val; // Right
        }
        ssize_t written = write(fd, buf, n * HAKIOS_AUDIO_CHANNELS * sizeof(int16_t));
        if (written <= 0) {
            perror("write");
            break;
        }
        frames_left -= n;
        current_frame += n;
    }
    free(buf);
    close(fd);
    printf("Playback complete: %d frames written.\n", current_frame);
    return 0;
}

static int set_vol(uint32_t db_eighths, int muted) {
    int fd = open(HAKIOS_AUDIO_DEVICE, O_WRONLY);
    if (fd < 0) {
        perror("open " HAKIOS_AUDIO_DEVICE);
        return 1;
    }
    struct hakios_audio_volume v = { .below = db_eighths, .muted = (uint32_t)muted };
    if (ioctl(fd, HAKIOS_AUDIO_SET_VOLUME, &v) < 0) {
        perror("ioctl HAKIOS_AUDIO_SET_VOLUME");
        close(fd);
        return 1;
    }
    printf("Volume set: %.2f dB below max, muted=%d\n", db_eighths / 8.0, muted);
    close(fd);
    return 0;
}

static int record_mic(float duration) {
    int fd = open(HAKIOS_MIC_DEVICE, O_RDONLY);
    if (fd < 0) {
        perror("open " HAKIOS_MIC_DEVICE);
        return 1;
    }
    printf("Recording from %s for %.1f s...\n", HAKIOS_MIC_DEVICE, duration);

    int total_samples = (int)(duration * HAKIOS_AUDIO_RATE);
    int chunk_samples = 512;
    int16_t buf[512];

    int samples_read = 0;
    int max_amp = 0;
    while (samples_read < total_samples) {
        int to_read = (total_samples - samples_read < chunk_samples) ?
                      (total_samples - samples_read) : chunk_samples;
        ssize_t n = read(fd, buf, to_read * sizeof(int16_t));
        if (n <= 0) {
            perror("read");
            break;
        }
        int num = n / sizeof(int16_t);
        for (int i = 0; i < num; i++) {
            int a = abs(buf[i]);
            if (a > max_amp) max_amp = a;
        }
        samples_read += num;
    }
    close(fd);
    printf("Recording finished: %d samples (%.2f s), peak amplitude %d / 32768 (%.1f%% FS)\n",
           samples_read, (float)samples_read / HAKIOS_AUDIO_RATE, max_amp, (max_amp * 100.0) / 32768.0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s play [freq] [duration_sec] [amp 0.0-1.0]\n", argv[0]);
        printf("       %s record [duration_sec]\n", argv[0]);
        printf("       %s vol [db_eighths] [muted 0/1]\n", argv[0]);
        return 1;
    }
    if (!strcmp(argv[1], "play")) {
        float freq = argc > 2 ? atof(argv[2]) : 440.0f;
        float dur = argc > 3 ? atof(argv[3]) : 2.0f;
        float amp = argc > 4 ? atof(argv[4]) : 0.8f;
        return play_tone(freq, dur, amp);
    } else if (!strcmp(argv[1], "record")) {
        float dur = argc > 2 ? atof(argv[2]) : 3.0f;
        return record_mic(dur);
    } else if (!strcmp(argv[1], "vol")) {
        uint32_t below = argc > 2 ? (uint32_t)atoi(argv[2]) : 0;
        int muted = argc > 3 ? atoi(argv[3]) : 0;
        return set_vol(below, muted);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    return 1;
}
