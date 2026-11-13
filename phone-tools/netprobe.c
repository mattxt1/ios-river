/* Read-only network inventory for River: interface names and IONetworkController services. */
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <IOKit/IOKitLib.h>

int main(void)
{
	struct ifaddrs *head = NULL;
	if (getifaddrs(&head) != 0) {
		perror("getifaddrs");
		return 1;
	}
	puts("interfaces:");
	for (struct ifaddrs *it = head; it != NULL; it = it->ifa_next) {
		if (it->ifa_name == NULL) continue;
		int seen = 0;
		for (struct ifaddrs *prev = head; prev != it; prev = prev->ifa_next) {
			if (prev->ifa_name != NULL && strcmp(prev->ifa_name, it->ifa_name) == 0) {
				seen = 1;
				break;
			}
		}
		if (!seen) printf("  %s flags=0x%x\n", it->ifa_name, it->ifa_flags);
	}
	puts("IPv4 addresses:");
	for (struct ifaddrs *it = head; it != NULL; it = it->ifa_next) {
		if (!it->ifa_name || !it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
		char address[INET_ADDRSTRLEN], mask[INET_ADDRSTRLEN] = "unknown";
		const struct sockaddr_in *sin = (const struct sockaddr_in *)it->ifa_addr;
		if (!inet_ntop(AF_INET, &sin->sin_addr, address, sizeof(address))) continue;
		if (it->ifa_netmask && it->ifa_netmask->sa_family == AF_INET) {
			const struct sockaddr_in *netmask = (const struct sockaddr_in *)it->ifa_netmask;
			(void)inet_ntop(AF_INET, &netmask->sin_addr, mask, sizeof(mask));
		}
		printf("  %s address=%s netmask=%s\n", it->ifa_name, address, mask);
	}
	freeifaddrs(head);

	CFMutableDictionaryRef match = IOServiceMatching("IONetworkController");
	io_iterator_t iterator = IO_OBJECT_NULL;
	if (match == NULL || IOServiceGetMatchingServices(kIOMainPortDefault, match, &iterator) != KERN_SUCCESS) {
		puts("IONetworkController query failed");
		return 1;
	}
	puts("IONetworkController services:");
	int count = 0;
	for (io_service_t service; (service = IOIteratorNext(iterator)) != IO_OBJECT_NULL;) {
		char name[128] = {0};
		IORegistryEntryGetName(service, name);
		printf("  %s\n", name);
		IOObjectRelease(service);
		count++;
	}
	if (count == 0) puts("  none");
	IOObjectRelease(iterator);
	return 0;
}
