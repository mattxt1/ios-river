/* Compile actual source extracted from0267 and proposed0290 with only hardware/
 * IOKit calls substituted. No phone access or policy writes from this test. */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
using vm_offset_t = uintptr_t;
static vm_offset_t ri_power_qg=1,ri_charge_chgr=2,ri_charge_dcdc=3,ri_power_usb=4;
static int ri_power_busy;
static uint16_t sample_voltage,sample_current;
static unsigned sample_path,sample_status,read_calls,fail_read,writes;
static bool fail_map;
static vm_offset_t ri_map(vm_offset_t pa) {return fail_map?0:pa;}
static vm_offset_t ri_spmi_obs(vm_offset_t,unsigned ppid) {return ppid;}
[[maybe_unused]] static vm_offset_t ri_spmi_chnl(vm_offset_t,unsigned ppid) {return ppid;}
[[maybe_unused]] static bool ri_spmi_write(vm_offset_t,unsigned addr,uint8_t val) {
 assert(addr==0x42&&val==1);writes++;return true;
}
static bool ri_power_read(vm_offset_t obs,unsigned addr,unsigned count,uint32_t *out) {
 read_calls++;if(read_calls==fail_read)return false;
 if(obs==ri_power_qg) {assert(addr==0xc0&&count==4);*out=sample_voltage|((uint32_t)sample_current<<16);}
 else if(obs==ri_charge_dcdc) {assert(addr==0x0b&&count==1);*out=sample_path;}
 else {assert(obs==ri_charge_chgr&&addr==6&&count==1);*out=sample_status;}
 return true;
}
extern "C" {
#include "battery-sample-added.h"
}
static char diagnostic_note[256];
#define SYSCTL_HANDLER_ARGS (void *oidp,void *req)
static int sysctl_handle_string(void *oidp,const char *note,int n,void *req) {
 (void)oidp;(void)n;(void)req;std::snprintf(diagnostic_note,sizeof(diagnostic_note),"%s",note);return 0;
}
#include "battery-diagnostic-added.h"
static bool PE_parse_boot_argn(const char *,void *,size_t) {return false;}
#define IOLog(...) ((void)0)
class SweetBattery {
public:
 bool fRiverHasSample=false;
 unsigned publications=0,notifications=0;
 std::map<std::string,long long> values;
 std::map<std::string,bool> flags;
 bool pollRiver(void);
 void putBool(const char *key,bool value) {flags[key]=value;}
 void updateStatus(void) {notifications++;}
 void publish(unsigned pct,unsigned mv,int ma,bool external,bool capable,bool charging,bool full,bool measured,bool currentValid) {
  publications++;values["percent"]=pct;values["mv"]=mv;values["ma"]=ma;
  flags["external"]=external;flags["capable"]=capable;flags["charging"]=charging;flags["full"]=full;
  flags["SweetBatteryHardwareSampleValid"]=measured;flags["SweetBatteryCurrentValid"]=currentValid;
  flags["SweetBatteryFallback"]=!measured;updateStatus();
 }
};
#include "battery-poll-added.h"
static void setup(unsigned mv=3800,int raw_ma=500) {
 ri_power_qg=1;ri_charge_chgr=2;ri_charge_dcdc=3;ri_power_usb=4;
 ri_power_busy=0;fail_read=read_calls=writes=0;fail_map=false;
#if !READONLY_SAMPLE
 ri_charge_cmd=0;
#endif
 sample_voltage=(uint16_t)(((uint64_t)mv*1000000+194636)/194637);
 /* Input QG polarity: positive discharge, output Apple negative discharge. */
 sample_current=(uint16_t)((int64_t)raw_ma*1000000/152588);
 sample_path=0;sample_status=0;
}
int main(void) {
 setup();SweetBattery b;assert(b.pollRiver());
 auto values=b.values;auto flags=b.flags;unsigned pct=(unsigned)values["percent"];
 assert(pct!=85&&b.flags["SweetBatteryHardwareSampleValid"]);
 ri_power_busy=1;assert(!b.pollRiver());
#if BUG_BASELINE
 assert(b.values["percent"]==85&&b.values["mv"]==4100&&b.values["ma"]==0);
 assert(b.flags["external"]&&b.flags["capable"]&&b.flags["full"]&&!b.flags["charging"]);
 setup();sample_current=0x8000;assert(b.pollRiver());
 assert(b.values["ma"]==0&&b.flags["SweetBatteryCurrentValid"]);
 printf("REPRODUCED baseline bugs: ordinary contention replaces %u%% with85%%/external/full; invalid current is accepted as valid0mA.\n",pct);
#else
 assert(b.values==values&&b.publications==1);
 assert(b.flags["external"]==flags["external"]&&b.flags["charging"]==flags["charging"]&&b.flags["full"]==flags["full"]);
 assert(!b.flags["SweetBatteryHardwareSampleValid"]&&!b.flags["SweetBatteryCurrentValid"]);
 assert(b.flags["SweetBatterySampleAvailable"]&&b.flags["SweetBatterySampleStale"]&&!b.flags["SweetBatteryFallback"]);
 for(unsigned failed=1;failed<=3;failed++) {
  setup();fail_read=failed;assert(!b.pollRiver());assert(b.values==values&&b.publications==1);
  assert(!b.flags["SweetBatteryHardwareSampleValid"]&&b.flags["SweetBatterySampleStale"]);
 }
 setup();sample_current=0x8000;sample_path=0x11;sample_status=7;
 assert(!b.pollRiver());assert(!writes&&b.values==values&&b.publications==1);
 setup();sample_voltage=0x8000;assert(!b.pollRiver());assert(b.values==values);
 setup();sample_voltage=0;assert(!b.pollRiver());assert(b.values==values);
 setup(2700);assert(!b.pollRiver());assert(b.values==values);
 setup(4700);assert(!b.pollRiver());assert(b.values==values);
 /* Never-valid source publishes no fake numeric/cable/full claims. */
 SweetBattery never;setup();ri_power_busy=1;assert(!never.pollRiver());
 assert(never.values.empty()&&!never.publications);
 assert(!never.flags.count("external")&&!never.flags.count("charging")&&!never.flags.count("full"));
 assert(!never.flags["SweetBatterySampleAvailable"]&&!never.flags["SweetBatterySampleStale"]);
 assert(!never.flags["SweetBatteryHardwareSampleValid"]&&!never.flags["SweetBatteryCurrentValid"]&&!never.flags["SweetBatteryFallback"]);
 /* Recovery publishes genuine fields and clears stale/invalid markers. */
 setup(3900,-100);sample_path=0x11;sample_status=2;assert(b.pollRiver());
 assert(b.publications==2&&b.values["percent"]!=pct&&b.values["ma"]>0);
 assert(b.flags["external"]&&b.flags["charging"]&&!b.flags["full"]);
 assert(b.flags["SweetBatteryHardwareSampleValid"]&&b.flags["SweetBatteryCurrentValid"]&&b.flags["SweetBatterySampleAvailable"]&&!b.flags["SweetBatterySampleStale"]);
 setup(3900,0);assert(never.pollRiver());assert(never.values["ma"]==0&&never.flags["SweetBatteryCurrentValid"]);
 assert(never.flags["SweetBatterySampleAvailable"]&&!never.flags["SweetBatterySampleStale"]);
 puts("PASS: first-sample unavailable, contention/read-failure retention, VI sentinel rejection, valid zero current and recovery flags");
#endif
 /* Exercise both the IOKit poll and actual debug.river_battery handler. */
 setup(3900,0);sample_path=0x11;sample_status=7;
#if !READONLY_SAMPLE
 ri_charge_cmd=5;
#endif
 SweetBattery disabled;assert(disabled.pollRiver());
 assert(disabled.flags["external"]&&!disabled.flags["charging"]&&!disabled.flags["full"]);
 assert(!river_battery_sysctl(nullptr,nullptr));
 assert(std::strstr(diagnostic_note,"external=1 charging=0 full=0"));
#if READONLY_SAMPLE
 assert(writes==0);
 puts("PASS: IOKit poll and actual battery diagnostic perform no charger writes on external+DISABLE_CHARGE");
#else
 assert(writes==2);
 puts("REPRODUCED pre0291 behavior: both poll and diagnostic write CHARGING_ENABLE for DISABLE_CHARGE");
#endif
 printf("Estimator evidence at3800mV: no-load=%u%%, -500mA=%u%%, -1000mA=%u%%; fixed IR sensitivity remains unmodified.\n",
     jg30_estimate_soc(3800,0),jg30_estimate_soc(3800,-500),jg30_estimate_soc(3800,-1000));
}
