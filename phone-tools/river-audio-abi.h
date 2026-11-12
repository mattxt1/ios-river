#ifndef _SYS_HAKIOS_AUDIO_H_
#define _SYS_HAKIOS_AUDIO_H_

#include <stdint.h>
#include <sys/ioctl.h>

#define HAKIOS_AUDIO_DEVICE    "/dev/hakios-audio"
#define HAKIOS_MIC_DEVICE      "/dev/hakios-mic"

#define HAKIOS_AUDIO_RATE      48000
#define HAKIOS_AUDIO_CHANNELS  2
#define HAKIOS_AUDIO_FRAME     4

struct hakios_audio_volume {
	uint32_t below; /* eighths of a dB */
	uint32_t muted;
};
#define HAKIOS_AUDIO_SET_VOLUME _IOW('r', 1, struct hakios_audio_volume)

/* Versioned diagnostic ABI: counters describe software traffic, never proof
 * of physical audio. A future transport must explicitly advertise real PCM. */
#define HAKIOS_AUDIO_STATUS_VERSION 1
#define HAKIOS_AUDIO_PLAYBACK_DISCARD (1u << 0)
#define HAKIOS_AUDIO_CAPTURE_SYNTHETIC (1u << 1)
struct hakios_audio_status {
 uint32_t version;
 uint32_t flags;
 uint64_t output_frames;
 uint64_t input_frames;
};
#define HAKIOS_AUDIO_GET_STATUS _IOR('r', 2, struct hakios_audio_status)

#endif /* _SYS_HAKIOS_AUDIO_H_ */
