/* Request a saved-network association from iOS when River stays unassociated.
 * Credentials and network selection remain with MobileWiFi and securityd. */
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

struct apple80211_request {
	char interface[16];
	int32_t type, value;
	uint32_t length;
	uint64_t data __attribute__((aligned(8)));
};
_Static_assert(sizeof(struct apple80211_request) == 40, "Apple80211 request size");

struct mobile_wifi {
	CFTypeRef (*create)(CFAllocatorRef, int);
	CFArrayRef (*devices)(CFTypeRef);
	CFArrayRef (*known)(CFTypeRef);
	bool (*enabled)(CFTypeRef);
	bool (*wait_for_user)(CFTypeRef);
	CFDateRef (*disabled_until)(CFTypeRef);
	CFTypeRef (*property)(CFTypeRef, CFStringRef);
	bool (*power)(CFTypeRef);
	uint8_t (*auto_join)(CFTypeRef);
	CFTypeRef (*current)(CFTypeRef);
	int (*associate)(CFTypeRef, CFTypeRef, void *, void *);
};

static int
association_state(int fd)
{
	struct apple80211_request request = {.interface = "en0", .type = 13};
	return ioctl(fd, 0xc02869c9u, &request) == 0 ? request.value : -1;
}

static bool
request_saved_join(const struct mobile_wifi *wifi, int fd, int *status)
{
	CFTypeRef manager = wifi->create(kCFAllocatorDefault, 0);
	if (!manager) {
		return false;
	}

	bool requested = false;
	CFArrayRef devices = NULL, known = NULL;
	if (wifi->auto_join(manager) != 1) {
		goto done;
	}
	devices = wifi->devices(manager);
	known = wifi->known(manager);
	if (!devices || !known || CFArrayGetCount(devices) != 1 || CFArrayGetCount(known) != 1) {
		goto done;
	}
	CFTypeRef device = CFArrayGetValueAtIndex(devices, 0);
	CFTypeRef network = CFArrayGetValueAtIndex(known, 0);
	if (!wifi->power(device) || !wifi->enabled(network) || wifi->wait_for_user(network) ||
	    !wifi->property(network, CFSTR("lastJoined"))) {
		goto done;
	}
	CFDateRef disabled = wifi->disabled_until(network);
	if (disabled && (CFGetTypeID(disabled) != CFDateGetTypeID() ||
	    CFDateGetAbsoluteTime(disabled) > CFAbsoluteTimeGetCurrent())) {
		goto done;
	}
	CFTypeRef current = wifi->current(device);
	if (current) {
		CFRelease(current);
		goto done;
	}
	int state = association_state(fd);
	if ((state == 0 || state == 1) && wifi->auto_join(manager) == 1) {
		*status = wifi->associate(device, network, NULL, NULL);
		requested = true;
		/* Keep the native client alive while wifid owns the async join. */
		if (*status == 0) {
			for (unsigned wait = 0; wait < 18 && association_state(fd) != 4; ++wait) {
				sleep(1);
			}
		}
	}

done:
	if (known) {
		CFRelease(known);
	}
	if (devices) {
		CFRelease(devices);
	}
	CFRelease(manager);
	return requested;
}

int
main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	void *framework = dlopen("/System/Library/PrivateFrameworks/MobileWiFi.framework/MobileWiFi",
	    RTLD_NOW | RTLD_LOCAL);
	void *keybag = dlopen("/System/Library/PrivateFrameworks/MobileKeyBag.framework/MobileKeyBag",
	    RTLD_NOW | RTLD_LOCAL);
	if (!framework || !keybag) {
		return 1;
	}
	int (*unlocked)(void) = dlsym(keybag, "MKBDeviceUnlockedSinceBoot");
	const struct mobile_wifi wifi = {
		.create = dlsym(framework, "WiFiManagerClientCreate"),
		.devices = dlsym(framework, "WiFiManagerClientCopyDevices"),
		.known = dlsym(framework, "WiFiManagerClientCopyNetworks"),
		.enabled = dlsym(framework, "WiFiNetworkIsEnabled"),
		.wait_for_user = dlsym(framework, "WiFiNetworkIsAutoJoinDisabledUntilFirstUserJoin"),
		.disabled_until = dlsym(framework, "WiFiNetworkGetDisabledUntilDate"),
		.property = dlsym(framework, "WiFiNetworkGetProperty"),
		.power = dlsym(framework, "WiFiDeviceClientGetPower"),
		.auto_join = dlsym(framework, "WiFiManagerClientGetUserAutoJoinState"),
		.current = dlsym(framework, "WiFiDeviceClientCopyCurrentNetwork"),
		.associate = dlsym(framework, "WiFiDeviceClientAssociateAsync"),
	};
	if (!unlocked || !wifi.create || !wifi.devices || !wifi.known || !wifi.enabled ||
	    !wifi.wait_for_user || !wifi.disabled_until || !wifi.property || !wifi.power ||
	    !wifi.auto_join || !wifi.current || !wifi.associate) {
		fputs("River saved Wi-Fi join: native interface unavailable\n", stderr);
		return 1;
	}
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		return 1;
	}
	unsigned idle_samples = 0, log_remaining = 8;
	unsigned retry_delay = 30;
	time_t next_check = 0;
	printf("River saved Wi-Fi join: started pid=%d\n", getpid());
	for (;; sleep(5)) {
		int state = association_state(fd);
		if (state == 4) {
			retry_delay = 30;
			next_check = 0;
		}
		if ((state != 0 && state != 1) || unlocked() != 1) {
			idle_samples = 0;
			continue;
		}
		if (idle_samples < 4 && ++idle_samples < 4) {
			continue;
		}
		time_t now = (time_t)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000000000u);
		if (now < next_check) {
			continue;
		}
		int status = 0;
		if (!request_saved_join(&wifi, fd, &status)) {
			next_check = now + 10;
			continue;
		}
		if (log_remaining) {
			--log_remaining;
			printf("River saved Wi-Fi join: request_status=%d\n", status);
		}
		next_check = now + retry_delay;
		if (retry_delay < 120) {
			retry_delay *= 2;
		}
	}
}
