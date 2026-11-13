/* Read-only scalar queries using River's existing Apple80211 ioctl ABI.
 * No scan, join, credential, SSID or BSSID operation is performed. */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

struct river_apple80211_request {
 char name[16];
 int32_t type, value;
 uint32_t length;
 uint64_t data __attribute__((aligned(8)));
};
_Static_assert(sizeof(struct river_apple80211_request)==40,"Apple80211 request size");
_Static_assert(offsetof(struct river_apple80211_request,data)==32,"Apple80211 data offset");

int main(void)
{
 int fd=socket(AF_INET,SOCK_DGRAM,0);
 if (fd<0) { perror("socket"); return 1; }
 const int types[]={13,21};
 const char *labels[]={"state","association_result"};
 for (unsigned i=0;i<sizeof(types)/sizeof(types[0]);i++) {
  struct river_apple80211_request request={0};
  memcpy(request.name,"en0",4); request.type=types[i];
  if (ioctl(fd,0xc02869c9u,&request)) { perror(labels[i]); close(fd); return 1; }
  printf("interface=en0 %s=%d\n",labels[i],request.value);
 }
 close(fd); return 0;
}
