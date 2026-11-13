#include "river-adsp-fixture.h"
int main(void) {
 uint8_t data[7516];for(unsigned i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i*7);
 setup();assert(!ri_adsp_metadata_auth(data,sizeof(data)));assert(secure_calls==1&&allocations==1&&cleaned==1&&scm_calls==4);
 assert(ri_adsp_metadata_attempted&&strstr(ri_adsp_note,"metadata_accepted=1"));
 assert(strstr(ri_adsp_note,"firmware_start_requested=0 firmware_memory_written=0"));
 assert(ri_adsp_metadata_auth(data,sizeof(data))==EALREADY&&secure_calls==1&&allocations==1&&scm_calls==4);
 setup();data[0]^=1;assert(ri_adsp_metadata_auth(data,sizeof(data))==EINVAL);data[0]^=1;
 assert(!secure_calls&&!allocations&&!scm_calls);
 setup();assert(ri_adsp_metadata_auth(data,sizeof(data)-1)==EINVAL&&!scm_calls);
 setup();gpu_ready=false;assert(ri_adsp_metadata_auth(data,sizeof(data))==EBUSY&&!allocations&&!scm_calls);
 setup();scenario=1;assert(ri_adsp_metadata_auth(data,sizeof(data))==EBUSY&&!allocations&&!secure_calls);
 setup();river_smd_smem=0;assert(ri_adsp_metadata_auth(data,sizeof(data))==EBUSY&&!allocations&&!secure_calls);
 setup();word(0x4000+28,1);assert(ri_adsp_metadata_auth(data,sizeof(data))==EBUSY&&!allocations&&!secure_calls);
 setup();alloc_fail=true;assert(ri_adsp_metadata_auth(data,sizeof(data))==ENOMEM&&!secure_calls);
 setup();physical=0x8d600000;assert(ri_adsp_metadata_auth(data,sizeof(data))==EIO&&!secure_calls&&!cleaned);
 setup();secure_status=-1;assert(ri_adsp_metadata_auth(data,sizeof(data))==EIO&&secure_calls==1&&ri_adsp_metadata_attempted);
 assert(ri_adsp_metadata_auth(data,sizeof(data))==EALREADY&&secure_calls==1);
 setup();secure_result=1;assert(ri_adsp_metadata_auth(data,sizeof(data))==EIO&&ri_adsp_metadata_attempted);
 puts("PASS: stock metadata digest, crypto/PAS/channel guards, kernel-RAM bounds, cache clean, only INIT_IMAGE and retained one-shot on failure");
}
