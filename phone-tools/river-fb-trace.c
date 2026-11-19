/* Root-only metadata trace; no pixels or app text. --start resets, --stop
 * freezes, --read prints the last 256 present attempts. Kernel self-stops at
 * 30 seconds or 1200 attempts. These are not unique displayed-frame counts. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <IOKit/IOKitLib.h>
#include "river_fb_composition_trace.h"
static void rect(const uint32_t r[4]) {
 printf("[%d,%d,%d,%d]",(int32_t)r[0],(int32_t)r[1],(int32_t)r[2],(int32_t)r[3]);
}
int main(int argc,char **argv) {
 if(argc!=2 || (strcmp(argv[1],"--start") && strcmp(argv[1],"--stop") && strcmp(argv[1],"--read"))) {
  fprintf(stderr,"usage: river-fb-trace --start|--stop|--read\n");return 2;
 }
 io_service_t service=IOServiceGetMatchingService(MACH_PORT_NULL,IOServiceMatching("AppleCLCD"));
 io_connect_t connection=0;if(!service){fprintf(stderr,"AppleCLCD unavailable\n");return 1;}
 kern_return_t kr=IOServiceOpen(service,mach_task_self(),0,&connection);IOObjectRelease(service);
 if(kr){fprintf(stderr,"open: 0x%x\n",kr);return 1;}
 if(strcmp(argv[1],"--read")) {
  uint64_t command=!strcmp(argv[1],"--start");
  uint64_t first=0;struct river_fb_trace_packet probe={0};size_t probeSize=sizeof(probe);
  kr=IOConnectCallMethod(connection,RIVER_FB_TRACE_READ,&first,1,NULL,0,NULL,NULL,&probe,&probeSize);
  if((kr==kIOReturnBusy && command) || (kr!=kIOReturnBusy &&
      (kr || probeSize!=sizeof(probe) || probe.version!=1))) {
   fprintf(stderr,"trace already running or compatible kernel unavailable (0x%x)\n",kr);
   IOServiceClose(connection);return 1;
  }
  kr=IOConnectCallScalarMethod(connection,RIVER_FB_TRACE_CONTROL,&command,1,NULL,NULL);
  if(!kr)printf("trace %s\n",command?"started (30s/1200 attempts maximum)":"stopped");
 } else {
  uint64_t first=0,expected=0;int begun=0;
  do {
   struct river_fb_trace_packet p={0};size_t size=sizeof(p);
   kr=IOConnectCallMethod(connection,RIVER_FB_TRACE_READ,&first,1,NULL,0,NULL,NULL,&p,&size);
   if(kr)break;
   if(size!=sizeof(p)||p.version!=1||p.available>256||p.count>16||p.first!=first||
       first+p.count>p.available||(first<p.available&&!p.count)||(begun&&expected!=p.total)) {
    fprintf(stderr,"invalid or changed trace snapshot\n");kr=kIOReturnError;break;
   }
   if(!begun){expected=p.total;printf("{\"kind\":\"present_attempts\",\"total\":%llu,\"retained\":%u,\"records\":[",(unsigned long long)p.total,p.available);begun=1;}
   for(unsigned j=0;j<p.count;j++) {
    struct river_fb_trace_record *r=&p.records[j];if(first+j)putchar(',');
    printf("{\"ordinal\":%llu,\"time_ns\":%llu,\"swap\":%u,\"mask\":%u,\"layers\":[",(unsigned long long)r->ordinal,(unsigned long long)r->time_ns,r->swap_id,r->mask);
    for(unsigned i=0;i<4;i++){if(i)putchar(',');printf("{\"surface\":%u,\"source\":",r->surface[i]);rect(r->source_rect[i]);printf(",\"destination\":");rect(r->destination_rect[i]);putchar('}');}
    printf("]}");
   }
   first+=p.count;if(first>=p.available)break;
  }while(first<256);
  if(begun)puts("]}");
 }
 if(kr)fprintf(stderr,"trace method: 0x%x (read requires a stopped trace)\n",kr);
 IOServiceClose(connection);return kr?1:0;
}
