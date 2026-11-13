/* Read-only River IPv4/SC diagnostics. Output contains private network addresses.
 * Does not acquire a lease, change routes, publish SC state or inspect credentials. */
#include <CoreFoundation/CoreFoundation.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <net/if.h>
#include <net/route.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

_Static_assert(sizeof(struct rt_msghdr)==92,"Darwin route dump ABI");
typedef const struct __SCDynamicStore *Store;
static void print_plist(const char *label, CFPropertyListRef value) {
 printf("%s:\n",label);
 if(!value){puts("  absent");return;}
 CFErrorRef error=NULL;
 CFDataRef data=CFPropertyListCreateData(NULL,value,kCFPropertyListXMLFormat_v1_0,0,&error);
 if(data){fwrite(CFDataGetBytePtr(data),1,(size_t)CFDataGetLength(data),stdout);CFRelease(data);}
 else puts("  plist serialization failed");
 if(error)CFRelease(error);
}
static void route_dump(void) {
 int mib[]={CTL_NET,PF_ROUTE,0,AF_INET,NET_RT_DUMP,0};size_t size=0;
 puts("IPv4 route table:");
 if(sysctl(mib,6,NULL,&size,NULL,0)||size>2*1024*1024){perror("route size");return;}
 if(!size){puts("  empty");return;}
 unsigned char *buf=malloc(size);if(!buf)return;
 if(sysctl(mib,6,buf,&size,NULL,0)){perror("route dump");free(buf);return;}
 size_t off=0;unsigned entries=0;
 while(off+sizeof(struct rt_msghdr)<=size && entries++<512){
  struct rt_msghdr h;memcpy(&h,buf+off,sizeof(h));
  if(h.rtm_msglen<sizeof(h)||h.rtm_msglen>size-off){puts("  malformed route record");break;}
  char name[IF_NAMESIZE]={0};(void)if_indextoname(h.rtm_index,name);
  printf("  if=%s index=%u flags=%#x addrs=%#x",name,h.rtm_index,h.rtm_flags,h.rtm_addrs);
  size_t pos=off+sizeof(h),end=off+h.rtm_msglen;
  for(unsigned bit=1;bit<=RTA_BRD;bit<<=1){
   if(!(h.rtm_addrs&bit))continue;
   if(pos+2>end){printf(" malformed-address");break;}
   unsigned len=buf[pos],step=len?(len+3)&~3u:4;
   if(len>end-pos||step>end-pos){printf(" malformed-address");break;}
   const char *label=bit==RTA_DST?"dst":bit==RTA_GATEWAY?"gateway":bit==RTA_NETMASK?"mask":NULL;
   if(label){
    /* Darwin may encode a short mask. Zero-fill omitted address bytes. */
    unsigned char ip[4]={0};char text[INET_ADDRSTRLEN];
    if((buf[pos+1]==AF_INET || (bit==RTA_NETMASK && buf[pos+1]==0)) && len>=4){
     unsigned n=len-4;if(n>4)n=4;memcpy(ip,buf+pos+4,n);
     if(inet_ntop(AF_INET,ip,text,sizeof(text)))printf(" %s=%s",label,text);
    }else printf(" %s-family=%u",label,buf[pos+1]);
   }
   pos+=step;
  }
  putchar('\n');off+=h.rtm_msglen;
 }
 free(buf);
}
static void selected_options(const char *label,CFPropertyListRef value) {
 if(!value||CFGetTypeID(value)!=CFDictionaryGetTypeID()){print_plist(label,NULL);return;}
 CFMutableDictionaryRef selected=CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
 const CFStringRef fields[]={CFSTR("InterfaceName"),CFSTR("DeviceName"),CFSTR("ServiceID"),CFSTR("PrimaryRank"),CFSTR("ServiceIndex"),CFSTR("IPIsCoupled"),CFSTR("IPIsIndependent"),CFSTR("OverridePrimary"),CFSTR("Scoped"),CFSTR("ExcludeFromNWI"),CFSTR("IsNULL")};
 for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++){
  CFTypeRef field=CFDictionaryGetValue(value,fields[i]);
  if(field&&(CFGetTypeID(field)==CFStringGetTypeID()||CFGetTypeID(field)==CFNumberGetTypeID()||CFGetTypeID(field)==CFBooleanGetTypeID()))CFDictionarySetValue(selected,fields[i],field);
 }
 print_plist(label,selected);CFRelease(selected);
}
static void rank_assertions(void *handle) {
 /* Apple's SCNetworkConfigurationPrivate.h: CopyRankAssertionInfo reports
  * other processes' assertions; GetPrimaryRank alone does not. */
 CFTypeRef (*create)(CFAllocatorRef,CFStringRef,uint32_t)=dlsym(handle,"_SCNetworkInterfaceCreateWithBSDName");
 CFArrayRef (*copy)(CFTypeRef)=dlsym(handle,"SCNetworkInterfaceCopyRankAssertionInfo");
 uint32_t (*rank)(CFTypeRef)=dlsym(handle,"SCNetworkInterfaceRankAssertionInfoGetPrimaryRank");
 pid_t (*pid)(CFTypeRef)=dlsym(handle,"SCNetworkInterfaceRankAssertionInfoGetProcessID");
 CFStringRef (*name)(CFTypeRef)=dlsym(handle,"SCNetworkInterfaceRankAssertionInfoGetProcessName");
 if(!create||!copy||!rank||!pid){puts("interface rank read SPI unavailable");return;}
 const CFStringRef interfaces[]={CFSTR("en0"),CFSTR("ue0")};
 for(unsigned i=0;i<2;i++){
  CFTypeRef interface=create(NULL,interfaces[i],0);
  if(!interface){printf("rank interface=%s unavailable\n",i?"ue0":"en0");continue;}
  CFArrayRef assertions=copy(interface);CFIndex n=assertions?CFArrayGetCount(assertions):0;
  printf("rank interface=%s assertion_count=%ld\n",i?"ue0":"en0",(long)n);
  if(n>64)n=64;
  for(CFIndex j=0;j<n;j++){
   CFTypeRef item=CFArrayGetValueAtIndex(assertions,j);char process[128]="<unavailable>";
   CFStringRef process_name=name?name(item):NULL;
   if(process_name)(void)CFStringGetCString(process_name,process,sizeof(process),kCFStringEncodingUTF8);
   printf("  rank=%u pid=%d process=%s\n",rank(item),pid(item),process);
  }
  if(assertions)CFRelease(assertions);CFRelease(interface);
 }
}
static void dynamic_state(void) {
 void *handle=dlopen("/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration",RTLD_LAZY);
 if(!handle){puts("SystemConfiguration unavailable");return;}
 Store (*create)(CFAllocatorRef,CFStringRef,void*,void*)=dlsym(handle,"SCDynamicStoreCreate");
 CFArrayRef (*keys)(Store,CFStringRef)=dlsym(handle,"SCDynamicStoreCopyKeyList");
 CFPropertyListRef (*copy)(Store,CFStringRef)=dlsym(handle,"SCDynamicStoreCopyValue");
 if(!create||!keys||!copy){puts("SC read symbols unavailable");dlclose(handle);return;}
 rank_assertions(handle);
 Store store=create(NULL,CFSTR("river-network-config-state"),NULL,NULL);
 if(!store){puts("SC read session unavailable");dlclose(handle);return;}
 const CFStringRef patterns[]={
  CFSTR("^(State|Setup):/Network/Global/(IPv4|DNS)$"),
  CFSTR("^(State|Setup):/Network/Service/[^/]+/(IPv4|IPv6|DNS|Interface)$"),
  CFSTR("^State:/Network/Interface/(en0|ue0)/(IPv4|Link)$"),
  CFSTR("^(State|Setup):/Network/Service/[^/]+$")
 };
 for(unsigned p=0;p<sizeof(patterns)/sizeof(patterns[0]);p++){
  CFArrayRef found=keys(store,patterns[p]);if(!found)continue;
  CFIndex count=CFArrayGetCount(found);if(count>128)count=128;
  for(CFIndex i=0;i<count;i++){
   CFStringRef key=CFArrayGetValueAtIndex(found,i);char label[512];
   if(CFGetTypeID(key)!=CFStringGetTypeID()||!CFStringGetCString(key,label,sizeof(label),kCFStringEncodingUTF8))continue;
   CFPropertyListRef value=copy(store,key);
   if(p==3)selected_options(label,value);else print_plist(label,value);
   if(value)CFRelease(value);
  }
  CFRelease(found);
 }
 CFRelease(store);dlclose(handle);
}
static void preferences(void) {
 FILE *file=fopen("/Library/Preferences/SystemConfiguration/preferences.plist","rb");
 if(!file){perror("preferences read");return;}
 if(fseek(file,0,SEEK_END)){fclose(file);return;}long size=ftell(file);
 if(size<=0||size>1024*1024||fseek(file,0,SEEK_SET)){puts("preferences size rejected");fclose(file);return;}
 unsigned char *buf=malloc((size_t)size);if(!buf){fclose(file);return;}
 size_t got=fread(buf,1,(size_t)size,file);fclose(file);
 CFDataRef data=CFDataCreate(NULL,buf,(CFIndex)got);free(buf);if(!data)return;
 CFPropertyListRef plist=CFPropertyListCreateWithData(NULL,data,kCFPropertyListImmutable,NULL,NULL);CFRelease(data);
 if(!plist)return;
 if(CFGetTypeID(plist)==CFDictionaryGetTypeID()){
  CFTypeRef services=CFDictionaryGetValue(plist,CFSTR("NetworkServices"));
  if(services&&CFGetTypeID(services)==CFDictionaryGetTypeID()){
   CFIndex n=CFDictionaryGetCount(services);
   if(n>0&&n<=128){
    const void **names=calloc((size_t)n,sizeof(void*)),**values=calloc((size_t)n,sizeof(void*));
    if(names&&values){
     CFDictionaryGetKeysAndValues(services,names,values);
     for(CFIndex i=0;i<n;i++){
      if(CFGetTypeID(names[i])!=CFStringGetTypeID()||CFGetTypeID(values[i])!=CFDictionaryGetTypeID())continue;
      char service[256];if(!CFStringGetCString(names[i],service,sizeof(service),kCFStringEncodingUTF8))continue;
      const CFStringRef selected[]={CFSTR("Interface"),CFSTR("IPv4"),CFSTR("DNS")};
      for(unsigned k=0;k<3;k++){
       char field[32],label[320];CFStringGetCString(selected[k],field,sizeof(field),kCFStringEncodingUTF8);
       snprintf(label,sizeof(label),"Preferences/NetworkServices/%s/%s",service,field);
       print_plist(label,CFDictionaryGetValue(values[i],selected[k]));
      }
     }
    }
    free(names);free(values);
   }
  }
 }
 CFRelease(plist);
}
int main(void) {route_dump();dynamic_state();preferences();return 0;}
