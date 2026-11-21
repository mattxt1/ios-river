/* One bounded synthetic gesture, or a downsampled compositor readback. */
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurfaceRef.h>
#include <dlfcn.h>
#include <mach/mach_time.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

typedef CFTypeRef HIDEvent;
typedef CFTypeRef HIDClient;
static HIDClient (*create_client)(CFAllocatorRef);
static HIDEvent (*hand_event)(CFAllocatorRef, uint64_t, uint32_t, uint32_t, uint32_t,
    uint32_t, uint32_t, double, double, double, double, double, bool, bool, uint32_t);
static HIDEvent (*finger_event)(CFAllocatorRef, uint64_t, uint32_t, uint32_t,
    uint32_t, double, double, double, double, double, bool, bool, uint32_t);
static void (*append_event)(HIDEvent, HIDEvent, uint32_t);
static void (*set_integer)(HIDEvent, uint32_t, CFIndex);
static void (*set_sender)(HIDEvent, uint64_t);
static void (*dispatch_event)(HIDClient, HIDEvent);

enum {
    Digitizer = 11 << 16, Collection = Digitizer | 22, DisplayIntegrated = Digitizer | 25,
    Range = 1 << 0, Touch = 1 << 1, Position = 1 << 2,
    Identity = 1 << 5, FromEdge = 1 << 11, Hand = 3,
};

static bool send_touch(HIDClient client, uint64_t sender, double x, double y,
    bool down, uint32_t mask)
{
    uint64_t now = mach_absolute_time();
    HIDEvent hand = hand_event(kCFAllocatorDefault, now, Hand, 0, 0, mask, down,
        x, y, 0, 0, 0, down, down, 0);
    HIDEvent finger = finger_event(kCFAllocatorDefault, now, 0, 2, mask, x, y,
        0, down ? 1.0 : 0, 0, down, down, 0);
    if (!hand || !finger) {
        if (hand) CFRelease(hand);
        if (finger) CFRelease(finger);
        return false;
    }
    set_integer(hand, Collection, 1);
    set_integer(hand, DisplayIntegrated, 1);
    append_event(hand, finger, 0);
    set_sender(hand, sender);
    dispatch_event(client, hand);
    CFRelease(finger);
    CFRelease(hand);
    return true;
}

static int gesture(double x, double y, double end_x, double end_y, unsigned ms, bool edge)
{
    void *iokit = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_LAZY);
    if (!iokit) return 1;
    create_client = dlsym(iokit, "IOHIDEventSystemClientCreate");
    hand_event = dlsym(iokit, "IOHIDEventCreateDigitizerEvent");
    finger_event = dlsym(iokit, "IOHIDEventCreateDigitizerFingerEvent");
    append_event = dlsym(iokit, "IOHIDEventAppendEvent");
    set_integer = dlsym(iokit, "IOHIDEventSetIntegerValue");
    set_sender = dlsym(iokit, "IOHIDEventSetSenderID");
    dispatch_event = dlsym(iokit, "IOHIDEventSystemClientDispatchEvent");
    if (!create_client || !hand_event || !finger_event || !append_event ||
        !set_integer || !set_sender || !dispatch_event) {
        fprintf(stderr, "HID client symbols unavailable\n");
        return 1;
    }
    io_service_t touch = IOServiceGetMatchingService(kIOMainPortDefault,
        IOServiceMatching("SweetHIDTouch"));
    uint64_t sender = 0;
    if (touch) {
        IORegistryEntryGetRegistryEntryID(touch, &sender);
        IOObjectRelease(touch);
    }
    if (!sender) {
        fprintf(stderr, "River digitizer sender unavailable\n");
        return 1;
    }
    HIDClient client = create_client(kCFAllocatorDefault);
    if (!client) return 1;
    unsigned steps = ms / 16;
    uint32_t edge_mask = edge ? FromEdge : 0;
    bool sent = send_touch(client, sender, x, y, true,
        edge_mask | Range | Touch | Position | Identity);
    for (unsigned i = 1; sent && i <= steps; i++) {
        usleep(ms * 1000 / steps);
        double progress = (double)i / steps;
        sent = send_touch(client, sender, x + (end_x - x) * progress,
            y + (end_y - y) * progress, true, edge_mask | Position);
    }
    if (sent) usleep(16000);
    bool lifted = send_touch(client, sender, end_x, end_y, false,
        edge_mask | Range | Touch);
    CFRelease(client);
    return sent && lifted ? 0 : 1;
}

static int screen(uint32_t surface_id)
{
    IOSurfaceRef surface = IOSurfaceLookup(surface_id);
    if (!surface) {
        fprintf(stderr, "Compositor surface %u is unavailable\n", surface_id);
        return 1;
    }
    size_t width = IOSurfaceGetWidth(surface), height = IOSurfaceGetHeight(surface);
    if (width < 600 || width > 1500 || height < 800 || height > 3000 ||
        IOSurfaceGetBytesPerElement(surface) != 4) {
        CFRelease(surface);
        fprintf(stderr, "Unexpected compositor surface geometry\n");
        return 1;
    }
    if (IOSurfaceLock(surface, kIOSurfaceLockReadOnly, NULL)) {
        CFRelease(surface);
        return 1;
    }
    const uint8_t *pixels = IOSurfaceGetBaseAddress(surface);
    size_t stride = IOSurfaceGetBytesPerRow(surface);
    unsigned out_w = width / 5, out_h = height / 5;
    size_t length = (size_t)out_w * out_h + 48;
    uint8_t *frame = pixels && stride >= width * 4 ? malloc(length) : NULL;
    bool nonblack = false;
    if (frame) {
        int header = snprintf((char *)frame, 48, "P5\n%u %u\n255\n", out_w, out_h);
        uint8_t *out = frame + header;
        for (unsigned y = 0; y < out_h; y++) {
            for (unsigned x = 0; x < out_w; x++) {
                const uint8_t *pixel = pixels + 5 * y * stride + 20 * x;
                *out++ = ((pixel[0] + 2 * pixel[1] + pixel[2]) / 4) & 0xf8;
                nonblack |= (pixel[0] | pixel[1] | pixel[2]) != 0;
            }
        }
        length = (size_t)(out - frame);
    }
    IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, NULL);
    CFRelease(surface);
    if (!nonblack) {
        free(frame);
        fprintf(stderr, "Compositor returned an all-black frame\n");
        return 1;
    }
    uLongf packed_size = compressBound(length);
    uint8_t *packed = malloc(packed_size);
    bool written = packed && compress2(packed, &packed_size, frame, length, Z_BEST_SPEED) == Z_OK &&
        fwrite(packed, 1, packed_size, stdout) == packed_size && !fflush(stdout);
    free(packed);
    free(frame);
    return written ? 0 : 1;
}

typedef struct {
    int pid;
    uint64_t flags, delta_timestamp, buffer;
    uint32_t size;
    uint64_t out_buffer_addr, out_size_addr, pagetable_mask;
} StackshotConfig;
_Static_assert(sizeof(StackshotConfig) == 64, "iOS 16 stackshot config ABI");

static int stackshot(int pid)
{
    int (*capture)(StackshotConfig *) = dlsym(RTLD_DEFAULT, "stackshot_capture_with_config");
    if (!capture) return 1;
    StackshotConfig config = {.pid = pid, .flags = 0x1010002}; // KCData, wait and load info.
    int error = capture(&config);
    if (error || !config.buffer || !config.size || config.size > 4 * 1024 * 1024) {
        fprintf(stderr, "Stackshot PID %d failed (%d, %u bytes)\n", pid, error, config.size);
        return 1;
    }
    return fwrite((void *)(uintptr_t)config.buffer, 1, config.size, stdout) == config.size &&
        !fflush(stdout) ? 0 : 1;
}

static bool number(const char *text, double min, double max, double *result)
{
    char *end;
    *result = strtod(text, &end);
    return end != text && !*end && isfinite(*result) && *result >= min && *result <= max;
}

/* The kernel publishes this only after the fit staging buffer is ready. */
static int viewport(void)
{
    io_service_t display = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleCLCD"));
    if (!display) return 1;
    CFTypeRef value = IORegistryEntryCreateCFProperty(display, CFSTR("RiverDisplayFit"), NULL, 0);
    IOObjectRelease(display);
    puts(value == kCFBooleanTrue ? "fit" : "crop");
    if (value) CFRelease(value);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "viewport")) return viewport();
    double value[5];
    if (argc == 3 && !strcmp(argv[1], "stackshot") &&
        number(argv[2], 1, INT32_MAX, &value[0]) && trunc(value[0]) == value[0])
        return stackshot((int)value[0]);
    if (argc == 3 && !strcmp(argv[1], "screen") &&
        number(argv[2], 1, UINT32_MAX, &value[0]) && trunc(value[0]) == value[0])
        return screen((uint32_t)value[0]);
    if ((argc == 7 || argc == 8) && !strcmp(argv[1], "gesture") &&
        (argc == 7 || !strcmp(argv[7], "edge"))) {
        for (unsigned i = 0; i < 4; i++)
            if (!number(argv[i + 2], 0, 1, &value[i])) return 2;
        if (number(argv[6], 32, 1200, &value[4]))
            return gesture(value[0], value[1], value[2], value[3], (unsigned)value[4], argc == 8);
    }
    fprintf(stderr, "usage: river-ui-replay viewport | screen SURFACE_ID | stackshot PID | gesture X Y X2 Y2 MS [edge]\n");
    return 2;
}
