#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

typedef kern_return_t (*SetPropsFn)(io_service_t, CFTypeRef);

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_brightness <nits: 1..500>\n");
        return 2;
    }
    int nits = atoi(argv[1]);
    if (nits < 1) nits = 1;
    if (nits > 500) nits = 500;

    SetPropsFn setProps = (SetPropsFn)dlsym(RTLD_DEFAULT, "IORegistryEntrySetCFProperties");
    if (!setProps) {
        fprintf(stderr, "IORegistryEntrySetCFProperties not found in IOKit\n");
        return 1;
    }

    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleCLCD"));
    if (!service) {
        fprintf(stderr, "AppleCLCD not found\n");
        return 1;
    }

    uint64_t val = (uint64_t)nits << 16;
    CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &val);
    const void *keys[] = { CFSTR("IOMFBBrightnessLevel") };
    const void *values[] = { num };
    CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);

    kern_return_t kr = setProps(service, dict);
    printf("set IOMFBBrightnessLevel to %d nits (0x%llx): kr = 0x%x\n", nits, (unsigned long long)val, kr);

    CFRelease(dict);
    CFRelease(num);
    IOObjectRelease(service);
    return (kr == KERN_SUCCESS) ? 0 : 1;
}
