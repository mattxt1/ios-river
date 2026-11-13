/* Read-only resolver metadata, no queries or configuration writes.
 * SPI layout: Apple configd/dnsinfo/dnsinfo.h (DNSINFO_VERSION 20170629),
 * https://github.com/apple-oss-distributions/configd/blob/main/dnsinfo/dnsinfo.h
 * Output contains private resolver IP addresses, but no domains/credentials.
 */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>

#pragma pack(push,4)
struct resolver {
 char *domain;int32_t n_nameserver;struct sockaddr **nameserver;
 uint16_t port;int32_t n_search;char **search;int32_t n_sortaddr;void *sortaddr;
 char *options;uint32_t timeout,search_order,if_index,flags,reach_flags,service_identifier;
 char *cid,*if_name;
};
struct configuration {
 int32_t n_resolver;struct resolver **resolver;
 int32_t n_scoped_resolver;struct resolver **scoped_resolver;
 uint64_t generation;
 int32_t n_service_specific_resolver;struct resolver **service_specific_resolver;
 uint32_t version;
};
#pragma pack(pop)
_Static_assert(sizeof(struct resolver)==96,"dnsinfo resolver arm64 ABI");
_Static_assert(sizeof(struct configuration)==48,"dnsinfo configuration arm64 ABI");
static void dump(const char *kind,int32_t count,struct resolver **list) {
 printf("%s resolvers=%d\n",kind,count);
 if(count<0||count>128||(!list&&count)){puts("resolver list bounds rejected");return;}
 for(int32_t i=0;i<count;i++){
  const struct resolver *r=list[i];if(!r){puts("  null resolver");continue;}
  printf("  resolver=%d if_index=%u flags=%#x A=%u AAAA=%u scoped=%u reach=%#x nameservers=%d\n",
   i,r->if_index,r->flags,(r->flags&2)!=0,(r->flags&4)!=0,(r->flags&0x1000)!=0,r->reach_flags,r->n_nameserver);
  if(r->n_nameserver<0||r->n_nameserver>32||(!r->nameserver&&r->n_nameserver))continue;
  for(int32_t j=0;j<r->n_nameserver;j++){
   const struct sockaddr *s=r->nameserver[j];char address[INET6_ADDRSTRLEN];const void *value=NULL;
   if(!s)continue;
   if(s->sa_family==AF_INET&&s->sa_len>=sizeof(struct sockaddr_in))value=&((const struct sockaddr_in*)s)->sin_addr;
   if(s->sa_family==AF_INET6&&s->sa_len>=sizeof(struct sockaddr_in6))value=&((const struct sockaddr_in6*)s)->sin6_addr;
   if(value&&inet_ntop(s->sa_family,value,address,sizeof(address)))printf("    nameserver=%s\n",address);
  }
 }
}
/* NWI remains opaque: use only Apple's published SPI accessors from
 * configd/nwi/network_information.h, never copy its private structure. */
static void nwi_dump(void) {
 void *(*copy)(void)=dlsym(RTLD_DEFAULT,"nwi_state_copy");
 void (*release)(void*)=dlsym(RTLD_DEFAULT,"nwi_state_release");
 void *(*lookup)(void*,const char*)=dlsym(RTLD_DEFAULT,"nwi_state_get_ifstate");
 uint64_t (*flags)(void*)=dlsym(RTLD_DEFAULT,"nwi_ifstate_get_flags");
 uint32_t (*reach)(void*,int)=dlsym(RTLD_DEFAULT,"nwi_state_get_reachability_flags");
 void *(*first)(void*,int)=dlsym(RTLD_DEFAULT,"nwi_state_get_first_ifstate");
 void *(*next)(void*,int)=dlsym(RTLD_DEFAULT,"nwi_ifstate_get_next");
 const char *(*name)(void*)=dlsym(RTLD_DEFAULT,"nwi_ifstate_get_ifname");
 if(!copy||!release||!lookup||!flags){puts("NWI read accessors unavailable");return;}
 void *state=copy();if(!state){puts("NWI: no published state");return;}
 if(reach)printf("NWI global IPv4 reach_flags=%#x IPv6 reach_flags=%#x\n",reach(state,AF_INET),reach(state,AF_INET6));
 const char *interfaces[]={"en0","ue0"};
 for(unsigned i=0;i<2;i++){
  void *ifstate=lookup(state,interfaces[i]);
  if(!ifstate){printf("NWI interface=%s absent\n",interfaces[i]);continue;}
  uint64_t value=flags(ifstate);
  printf("NWI interface=%s flags=%#llx IPv4=%u IPv6=%u DNS=%u\n",interfaces[i],(unsigned long long)value,
   (value&1)!=0,(value&2)!=0,(value&4)!=0);
 }
 if(first&&next&&name){
  void *current=first(state,AF_INET);unsigned count=0;
  while(current&&count<32){
   const char *ifname=name(current);
   printf("NWI general IPv4 rank=%u interface=%.16s\n",count,ifname?ifname:"<unknown>");
   current=next(current,AF_INET);count++;
  }
  if(!count)puts("NWI general IPv4 list empty");
 }
 release(state);
}
int main(void) {
 void *handle=dlopen("/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration",RTLD_LAZY|RTLD_GLOBAL);
 nwi_dump();
 struct configuration *(*copy)(void)=dlsym(RTLD_DEFAULT,"dns_configuration_copy");
 void (*release)(struct configuration*)=dlsym(RTLD_DEFAULT,"dns_configuration_free");
 if(!copy||!release){puts("dns_configuration SPI unavailable");if(handle)dlclose(handle);return 1;}
 struct configuration *c=copy();
 if(!c){puts("dns_configuration_copy: no published resolver configuration");if(handle)dlclose(handle);return 0;}
 printf("dnsinfo version=%u generation=%llu\n",c->version,(unsigned long long)c->generation);
 dump("default",c->n_resolver,c->resolver);
 dump("scoped",c->n_scoped_resolver,c->scoped_resolver);
 dump("service",c->n_service_specific_resolver,c->service_specific_resolver);
 release(c);if(handle)dlclose(handle);return 0;
}
