/* Read-only IPConfiguration summary client. Apple bootp-438 ipconfig.defs:
 * subsystem20000 get_summary index17; later native binary range20000..20022.
 * No setter, lease acquisition, rank change or DNS publication is exposed. */
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <mach/ndr.h>
#include <mach/mig.h>
#include <servers/bootstrap.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_media.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#pragma pack(push,4)
struct request {mach_msg_header_t head;NDR_record_t ndr;char name[16];};
struct response {mach_msg_header_t head;mach_msg_body_t body;mach_msg_ool_descriptor_t data;NDR_record_t ndr;uint32_t count;int32_t status;};
struct error_response {mach_msg_header_t head;NDR_record_t ndr;kern_return_t result;};
#pragma pack(pop)
_Static_assert(sizeof(struct request)==48,"MIG summary request ABI");
_Static_assert(sizeof(struct response)==60,"MIG summary response ABI");
static unsigned rows;
static int scalar_name(const char *s){const char *names[]={"InterfaceName","ServiceID","ConfigMethod","Method","Status","Active","Ready","LinkActive","LinkStatus","PrimaryRank","Published","DHCPStatus","InterfaceType","IPv4ServiceID","IPv6ServiceID","State","Mode","LastFailureStatus","IsPublished"};for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++)if(!strcmp(s,names[i]))return 1;return 0;}
static void summary(CFTypeRef value,const char *path,const char *key,unsigned depth){
 if(depth>8 || rows++>256)return;
 CFTypeID type=CFGetTypeID(value);
 if(type==CFDictionaryGetTypeID()){
  CFIndex n=CFDictionaryGetCount(value);if(n>128)return;const void *keys[128],*values[128];CFDictionaryGetKeysAndValues(value,keys,values);
  printf("%s dictionary count=%ld\n",path,(long)n);
  for(CFIndex i=0;i<n;i++)if(CFGetTypeID(keys[i])==CFStringGetTypeID()){
   char name[128],next[512];if(!CFStringGetCString(keys[i],name,sizeof(name),kCFStringEncodingUTF8))continue;
   snprintf(next,sizeof(next),"%s/%s",path,name);summary(values[i],next,name,depth+1);
  }
 }else if(type==CFArrayGetTypeID()){
  CFIndex n=CFArrayGetCount(value);printf("%s array count=%ld\n",path,(long)n);
  for(CFIndex i=0;i<n && i<16;i++){char next[512];snprintf(next,sizeof(next),"%s/%ld",path,(long)i);summary(CFArrayGetValueAtIndex(value,i),next,key,depth+1);}
 }else if(type==CFBooleanGetTypeID())printf("%s boolean=%u\n",path,CFBooleanGetValue(value)!=0);
 else if(type==CFNumberGetTypeID()){int64_t v=0;if(CFNumberGetValue(value,kCFNumberSInt64Type,&v))printf("%s number=%lld\n",path,(long long)v);}
 else if(type==CFStringGetTypeID() && scalar_name(key)){char s[256];if(CFStringGetCString(value,s,sizeof(s),kCFStringEncodingUTF8))printf("%s string=%s\n",path,s);}
 else printf("%s value_omitted=1\n",path);
}
int main(void){
 struct ifmediareq media;memset(&media,0,sizeof(media));memcpy(media.ifm_name,"en0",4);
 int sock=socket(AF_INET,SOCK_DGRAM,0);
 int e=sock<0?-1:ioctl(sock,SIOCGIFMEDIA,&media);
 printf("media_query=%d errno=%d count=%d current=0x%x status=0x%x valid=%u active=%u\n",e,e?errno:0,media.ifm_count,media.ifm_current,media.ifm_status,(media.ifm_count>0 && (media.ifm_status&IFM_AVALID))!=0,(media.ifm_status&IFM_ACTIVE)!=0);
 if(sock>=0)close(sock);
 mach_port_t server=MACH_PORT_NULL,reply=MACH_PORT_NULL;kern_return_t kr=bootstrap_look_up(bootstrap_port,"com.apple.network.IPConfiguration",&server);

 if(kr){printf("bootstrap_error=%d\n",kr);return 1;}
 reply=mig_get_reply_port();if(!MACH_PORT_VALID(reply)){return 1;}
 union {struct request req;struct response rep;struct error_response err;uint8_t space[512];} message;memset(&message,0,sizeof(message));
 message.req.head.msgh_bits=MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,MACH_MSG_TYPE_MAKE_SEND_ONCE);message.req.head.msgh_size=sizeof(message.req);message.req.head.msgh_remote_port=server;message.req.head.msgh_local_port=reply;message.req.head.msgh_id=20017;message.req.ndr=NDR_record;memcpy(message.req.name,"en0",4);
 kr=mach_msg(&message.req.head,MACH_SEND_MSG|MACH_RCV_MSG|MACH_SEND_TIMEOUT|MACH_RCV_TIMEOUT,sizeof(message.req),sizeof(message),reply,2000,MACH_PORT_NULL);
 int result=1;
 if(kr){printf("summary_transport_error=%d\n",kr);goto done;}
 if(message.rep.head.msgh_id!=20117){printf("unexpected_reply_id=%d\n",message.rep.head.msgh_id);goto destroy;}
 if(!(message.rep.head.msgh_bits&MACH_MSGH_BITS_COMPLEX)){
  if(message.err.head.msgh_size==sizeof(struct error_response))printf("summary_mig_error=%d\n",message.err.result);goto destroy;
 }
 if(message.rep.head.msgh_size!=sizeof(struct response) || message.rep.body.msgh_descriptor_count!=1 || message.rep.data.type!=MACH_MSG_OOL_DESCRIPTOR || message.rep.count!=message.rep.data.size || message.rep.count>65536 || memcmp(&message.rep.ndr,&NDR_record,sizeof(NDR_record))){puts("summary_invalid_reply=1");goto destroy;}
 printf("summary_status=%d bytes=%u\n",message.rep.status,message.rep.count);
 if(message.rep.status || !message.rep.count || !message.rep.data.address)goto destroy;
 CFDataRef data=CFDataCreate(NULL,message.rep.data.address,message.rep.count);if(!data)goto destroy;
 CFPropertyListRef plist=CFPropertyListCreateWithData(NULL,data,kCFPropertyListImmutable,NULL,NULL);CFRelease(data);
 if(plist){summary(plist,"IPConfiguration/en0","",0);CFRelease(plist);result=0;}
destroy:mach_msg_destroy(&message.rep.head);
done:/* Cached MIG reply port belongs to this short-lived process. */mach_port_deallocate(mach_task_self(),server);return result;
}
