#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleCLCD"));
    if (!service) {
        fprintf(stderr, "AppleCLCD not found\n");
        return 1;
    }
    io_connect_t conn = 0;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceOpen failed: 0x%x\n", kr);
        IOObjectRelease(service);
        return 1;
    }
    uint64_t in = 1; // 1 = power on
    kr = IOConnectCallScalarMethod(conn, 12, &in, 1, NULL, NULL);
    printf("IOConnectCallScalarMethod(12, 1) -> 0x%x\n", kr);
    IOServiceClose(conn);
    IOObjectRelease(service);
    return (kr == KERN_SUCCESS) ? 0 : 1;
}
