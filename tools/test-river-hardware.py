#!/usr/bin/env python3
"""Fault-inject actual patched kernel methods and native bridge hooks.

Usage: test-river-hardware.py APPLIED_XNU_SOURCE
No device access, charger writes, display changes or physical feedback.
"""
from pathlib import Path
import subprocess, sys, tempfile

repo = Path(__file__).resolve().parent.parent
if len(sys.argv) != 2:
    raise SystemExit(__doc__)
source = Path(sys.argv[1]).resolve()
keys = (source/'bsd/dev/sweet_keys.c').read_text()
fb = (source/'iokit/Kernel/arm/SweetFramebuffer.cpp').read_text()
battery = (source/'iokit/Kernel/arm/SweetBattery.cpp').read_text()
bridge = (repo/'phone-tools/river-hardware.m').read_text()

def function(text, name):
    start = text.index('\n'+name+'(')
    start = text.rfind('\n', 0, start-1)+1
    brace = text.index('{', start)
    depth, end = 1, brace+1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]+'\n'

battery_pre = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
using vm_offset_t=uintptr_t;
static vm_offset_t ri_power_qg=1,ri_charge_chgr=2,ri_charge_dcdc=3;
static int ri_power_busy;
static unsigned calls,fail_read,path_reads,state_reads,mappings;
static unsigned path=0x11,state=3,error_bits,temp,modes=1,command=1;
static uint32_t vi=19524u|(3276u<<16);
static bool fail_map,change_path,change_state;
static vm_offset_t ri_map(vm_offset_t a){mappings++;return fail_map?0:a;}
static vm_offset_t ri_spmi_obs(vm_offset_t,unsigned ppid){return ppid==0x248?1:ppid==0x210?2:3;}
static bool ri_power_read(vm_offset_t obs,unsigned reg,unsigned count,uint32_t *out){
 if(++calls==fail_read)return false;
 if(obs==1){assert(reg==0xc0&&count==4);*out=vi;return true;}
 assert(count==1);
 if(obs==3){assert(reg==0xb);*out=path ^ ((change_path&&++path_reads==2)?0x10:0);return true;}
 assert(obs==2);
 switch(reg){
 case 6:*out=state ^ ((change_state&&++state_reads==2)?1:0);break;
 case 7:*out=error_bits;break;case 0xd:*out=temp;break;
 case 0xb:*out=modes;break;case 0x42:*out=command;break;
 default:assert(false);
 }return true;
}
'''
battery_model = r'''
static bool PE_parse_boot_argn(const char*,void*,size_t){return false;}
#define IOLog(...) ((void)0)
class SweetBattery {
public:
 bool fRiverHasSample=false;
 unsigned publications=0;
 std::map<std::string,long long> values;
 std::map<std::string,bool> flags;
 bool pollRiver();
 void putBool(const char *k,bool v){flags[k]=v;}
 void putNumber(const char *k,unsigned v){values[k]=v;}
 void updateStatus(){}
 void publish(unsigned p,unsigned v,int a,bool e,bool cap,bool c,bool f,bool measured,bool current){
  publications++;values["percent"]=p;values["mv"]=v;values["ma"]=a;
  flags["external"]=e;flags["capable"]=cap;flags["charging"]=c;flags["full"]=f;
  flags["SweetBatteryHardwareSampleValid"]=measured;flags["SweetBatteryCurrentValid"]=current;
 }
};
'''
battery_main = r'''
static void reset(){calls=path_reads=state_reads=0;fail_read=0;ri_power_busy=0;path=0x11;state=3;error_bits=temp=0;modes=command=1;change_path=change_state=fail_map=false;}
int main(){
 river_battery_observation sample{};
 reset();assert(river_battery_observe(&sample));assert(sample.external&&sample.capable&&sample.charging&&!sample.full);assert(sample.ma<0);assert(calls==9);
 for(unsigned fail=1;fail<=9;fail++){
  reset();fail_read=fail;river_battery_observation old=sample;
  assert(!river_battery_observe(&sample));assert(!memcmp(&sample,&old,sizeof(sample)));assert(!ri_power_busy);
 }
 reset();ri_power_busy=1;unsigned before=mappings;ri_power_qg=0;
 assert(!river_battery_observe(&sample)&&mappings==before&&ri_power_busy==1);
 ri_power_busy=0;fail_map=true;assert(!river_battery_observe(&sample)&&!ri_power_busy);
 fail_map=false;assert(river_battery_observe(&sample)&&ri_power_qg==1&&!ri_power_busy);
 reset();change_path=true;assert(!river_battery_observe(&sample)&&!ri_power_busy);
 reset();change_state=true;assert(!river_battery_observe(&sample)&&!ri_power_busy);
 reset();uint32_t saved=vi;vi=0x8000u;assert(!river_battery_observe(&sample));
 vi=saved&0xffffu;assert(river_battery_observe(&sample)&&sample.ma==0);
 vi=(saved&0xffffu)|(0x8000u<<16);assert(!river_battery_observe(&sample));vi=saved;
 for(unsigned st=0;st<8;st++){
  reset();state=st;assert(river_battery_observe(&sample));
  assert(sample.charging==(st>=1&&st<=4));assert(sample.full==(st==5));
 }
 for(unsigned bit=0;bit<4;bit++){
  reset();temp=1u<<bit;assert(river_battery_observe(&sample));assert(!sample.capable&&!sample.charging&&!sample.full);
 }
 reset();temp=0x30;assert(river_battery_observe(&sample)&&sample.charging);
 reset();error_bits=2;state=5;assert(river_battery_observe(&sample)&&!sample.full&&!sample.capable);
 reset();modes=0;assert(river_battery_observe(&sample)&&!sample.charging);
 reset();command=0;assert(river_battery_observe(&sample)&&!sample.charging&&!sample.capable&&sample.external);
 reset();path=0x51;assert(river_battery_observe(&sample)&&sample.external&&!sample.charging);
 reset();path=0x49;assert(river_battery_observe(&sample)&&sample.external&&sample.charging);
 reset();path=0x10;assert(river_battery_observe(&sample)&&!sample.external&&!sample.charging);
 reset();path=0;assert(river_battery_observe(&sample)&&!sample.external&&!sample.charging);
 reset();SweetBattery b;assert(b.pollRiver());auto values=b.values;unsigned pubs=b.publications;
 fail_read=calls+1;assert(!b.pollRiver());assert(b.values==values&&b.publications==pubs&&b.flags["SweetBatterySampleStale"]);
 reset();state=7;command=0;assert(b.pollRiver());assert(b.flags["external"]&&!b.flags["capable"]&&!b.flags["charging"]&&!b.flags["full"]&&!b.flags["SweetBatterySampleStale"]);
 reset();SweetBattery empty;fail_read=1;assert(!empty.pollRiver()&&empty.values.empty()&&!empty.flags["SweetBatterySampleAvailable"]);
 puts("PASS: actual battery observer/poll: nine read failures, mapping/busy, transition rejection, sentinels, all charger states, thermal/error/mode/command/suspend/DC, stale/recovery; no write API linked");
}
'''

backlight_pre = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <climits>
#define RIVER 1
#define SWEET_FB_MAX_NITS 500
#define IOLog(...) ((void)0)
using IOReturn=int;using thread_call_param_t=void*;
enum{kIOReturnSuccess,kIOReturnUnsupported};
struct OSObject{virtual ~OSObject()=default;};
struct OSNumber:OSObject{uint64_t v;OSNumber(uint64_t x):v(x){}uint64_t unsigned64BitValue(){return v;}};
struct OSDictionary:OSObject{OSObject *n;OSObject *getObject(const char*){return n;}};
#define OSDynamicCast(T,v) dynamic_cast<T*>(v)
static bool lock1,lock2,transfer_ok=true;
static unsigned transfers,schedules,last_level;
static void IOLockLock(int l){if(l==1){assert(!lock1);lock1=true;}else{assert(l==2&&!lock1&&!lock2);lock2=true;}}
static void IOLockUnlock(int l){if(l==1){assert(lock1);lock1=false;}else{assert(lock2);lock2=false;}}
static void thread_call_enter(void*){schedules++;}
static void sweet_bl_log(const char*,OSDictionary*){}
class AppleCLCD{
public:
 int fLock=1,fBlLock=2;bool fPowerOff=false;uint32_t fBlLevel=0x999,fBlSent=~0u;
 void *fDsi=(void*)1,*fBlCall=(void*)1;
 std::map<std::string,uint64_t> props;
 void setProperty(const char*k,uint64_t v,int){props[k]=v;}
 bool dsiDcsLong(uint8_t *p,unsigned n){assert(lock2&&!lock1&&n==3&&p[0]==0x51);transfers++;last_level=((unsigned)p[1]<<8)|p[2];return transfer_ok;}
 void setPanelLevel(uint32_t);IOReturn setProperties(OSObject*);
 static void blWork(thread_call_param_t,thread_call_param_t);
};
'''
backlight_main = r'''
int main(){
 AppleCLCD fb;
 fb.setPanelLevel(1);assert(last_level==0x999&&fb.fBlSent==0x999);
 fb.setPanelLevel(2);assert(transfers==1); /* actual desired value wins over stale worker argument */
 fb.fPowerOff=true;fb.setPanelLevel(0x999);assert(last_level==0&&fb.fBlSent==0);
 fb.fBlLevel=0x123;AppleCLCD::blWork(&fb,nullptr);assert(last_level==0&&transfers==2);
 fb.fPowerOff=false;transfer_ok=false;AppleCLCD::blWork(&fb,nullptr);assert(last_level==0x123&&fb.fBlSent==UINT_MAX);
 transfer_ok=true;AppleCLCD::blWork(&fb,nullptr);assert(fb.fBlSent==0x123&&transfers==4);
 fb.fBlLevel=0x234;transfer_ok=false;AppleCLCD::blWork(&fb,nullptr);assert(fb.fBlSent==UINT_MAX&&transfers==5);
 fb.fBlLevel=0x123;transfer_ok=true;AppleCLCD::blWork(&fb,nullptr);assert(last_level==0x123&&fb.fBlSent==0x123&&transfers==6);
 for(uint64_t v: {0ULL,1ULL,250ULL<<16,500ULL<<16,UINT64_MAX}){
  OSNumber n(v);OSDictionary d;d.n=&n;assert(fb.setProperties(&d)==kIOReturnSuccess);
  uint64_t bounded=v>(500u<<16)?(500u<<16):v;
  assert(fb.props["IOMFBBrightnessLevel"]==bounded&&fb.props["IOMFBLinearBrightnessLevel"]==bounded);
  assert(fb.fBlLevel>=1&&fb.fBlLevel<=4095);
 }
 assert(schedules==5);
 OSObject bad;assert(fb.setProperties(&bad)==kIOReturnUnsupported);
 assert(!lock1&&!lock2);
 puts("PASS: actual backlight methods: queued stale request after sleep/wake, DMA failure/retry, duplicate coalescing, lock order, full-range/overflow clamp");
}
'''

hook_pre = r'''
#import <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <assert.h>
typedef double CGFloat;
typedef NS_ENUM(NSInteger,UINotificationFeedbackType){UINotificationFeedbackTypeSuccess,UINotificationFeedbackTypeWarning,UINotificationFeedbackTypeError};
enum{RIVER_FEEDBACK_SELECTION,RIVER_FEEDBACK_IMPACT,RIVER_FEEDBACK_SUCCESS,RIVER_FEEDBACK_WARNING,RIVER_FEEDBACK_ERROR};
static _Thread_local unsigned feedback_depth;
static unsigned originals,feedbacks,kind;static double amount;
static void feedback(unsigned k,double v){feedbacks++;kind=k;amount=v;}
@interface UIScreen:NSObject
@property CGFloat brightness;
+ (id)mainScreen;
@end
@implementation UIScreen
+ (id)mainScreen {static id screen;return screen?: (screen=[UIScreen new]);}
@end
static void brightness(double v){amount=v;}
@interface FeedbackBase:NSObject
- (void)selectionChanged;
- (void)impactOccurred;
- (void)impactOccurredWithIntensity:(CGFloat)v;
- (void)notificationOccurred:(UINotificationFeedbackType)t;
@end
@implementation FeedbackBase
- (void)selectionChanged{originals++;}
- (void)impactOccurred{originals++;[self impactOccurredWithIntensity:0.5];}
- (void)impactOccurredWithIntensity:(CGFloat)v{originals++;}
- (void)notificationOccurred:(UINotificationFeedbackType)t{originals++;}
@end
@interface Feedback:FeedbackBase @end
@implementation Feedback @end
'''
hook_main = r'''
int main(){@autoreleasepool{
 Class c=Feedback.class;
 assert(hook(c,@selector(selectionChanged),(IMP)selection,(IMP*)&selection_original,NULL));
 assert(!hook(c,@selector(selectionChanged),(IMP)selection,(IMP*)&selection_original,NULL));
 assert(hook(c,@selector(impactOccurred),(IMP)impact,(IMP*)&impact_original,NULL));
 assert(hook(c,@selector(impactOccurredWithIntensity:),(IMP)impact_intensity,(IMP*)&intensity_original,@encode(CGFloat)));
 assert(hook(c,@selector(notificationOccurred:),(IMP)notification,(IMP*)&notification_original,@encode(UINotificationFeedbackType)));
 Feedback *f=[Feedback new];[f selectionChanged];assert(originals==1&&feedbacks==1&&kind==RIVER_FEEDBACK_SELECTION);
 [f impactOccurred];assert(originals==3&&feedbacks==2&&kind==RIVER_FEEDBACK_IMPACT&&!feedback_depth);
 [f impactOccurredWithIntensity:.25];assert(feedbacks==3&&amount==.25);
 [f notificationOccurred:UINotificationFeedbackTypeError];assert(kind==RIVER_FEEDBACK_ERROR&&feedbacks==4);
 [f notificationOccurred:(UINotificationFeedbackType)10];assert(feedbacks==4);
 FeedbackBase *base=[FeedbackBase new];[base selectionChanged];assert(feedbacks==4); /* parent implementation remains intact */
 IMP wrong=NULL;assert(!hook(c,@selector(notificationOccurred:),(IMP)notification,&wrong,"d")&&!wrong);
 puts("PASS: actual Objective-C hooks: forwarding, nested-call deduplication, repeated install, inherited-method isolation and ABI mismatch rejection");
}}
'''

haptics_pre = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
static bool ri_haptics_ok=true,ri_native_feedback=true;
static uintptr_t ri_qup2=1;
static uint64_t ri_vib_deadline;
static int ri_vib_request=-1;
static unsigned writes,fail_write,last_reg,last_value,gain_value;
static bool ri_aw_wr(uint8_t reg,uint8_t value){
 writes++;last_reg=reg;last_value=value;if(reg==0x39)gain_value=value;
 return writes!=fail_write;
}
#define NSEC_PER_MSEC 1000000ULL
static uint64_t mach_absolute_time(void){return 1000;}
static void nanoseconds_to_absolutetime(uint64_t ns,uint64_t *out){*out=ns;}
struct request{void *newptr;};
#define SYSCTL_HANDLER_ARGS (void *oidp,void *arg1,int arg2,struct request *req)
static int requested,observed;
static int sysctl_handle_int(void *o,int *value,int a,struct request *req){observed=*value;if(req->newptr)*value=requested;return 0;}
'''
haptics_main = r'''
int main(void){
 struct request read={0},write={&requested};
 assert(!river_haptic_sysctl(0,0,0,&read)&&observed==1&&writes==0);
 ri_native_feedback=false;
 assert(!river_haptic_sysctl(0,0,0,&read)&&observed==0);
 requested=(0x20<<16)|25;assert(river_haptic_sysctl(0,0,0,&write)==ENXIO);
 ri_native_feedback=true;ri_haptics_ok=false;assert(river_haptic_sysctl(0,0,0,&write)==ENXIO);ri_haptics_ok=true;
 int invalid[]={-1,INT_MAX,1001,(0x29<<16)|25,25,0x20<<16};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){requested=invalid[i];assert(river_haptic_sysctl(0,0,0,&write)==EINVAL&&writes==0&&ri_vib_request==-1);}
 requested=(0x18<<16)|30;assert(!river_haptic_sysctl(0,0,0,&write)&&ri_vib_request==requested&&writes==0);
 unsigned packet=(unsigned)__atomic_exchange_n(&ri_vib_request,-1,__ATOMIC_ACQUIRE);
 river_haptic_play(packet&0xffff,packet>>16);assert(gain_value==0x18&&ri_vib_deadline==30001000&&writes==6);
 requested=0;assert(!river_haptic_sysctl(0,0,0,&write)&&ri_vib_request==0);
 river_haptic_play(0,0);assert(ri_vib_deadline==0&&last_reg==4&&last_value==1);
 requested=25;assert(!river_vibrate_sysctl(0,0,0,&write)&&ri_vib_request==((0x20<<16)|25));
 requested=1000;assert(!river_vibrate_sysctl(0,0,0,&write)&&ri_vib_request==((0x28<<16)|1000));
 for(unsigned fail=1;fail<=6;fail++){
  ri_haptics_ok=true;writes=0;fail_write=fail;river_haptic_play(30,0x20);
  assert(!ri_haptics_ok&&ri_vib_deadline==0&&last_reg==4&&last_value==1);
 }
 ri_haptics_ok=true;writes=fail_write=0;river_haptic_play(99999,99999);assert(gain_value==0x28&&ri_vib_deadline==1000001000);
 puts("PASS: actual native haptic endpoint/play: read-only readiness, disabled/unready, packet rejection, asynchronous mailbox/stop, legacy encoding, six write failures/standby and drive/duration caps");
}
'''

def sysctl_function(name):
    start=keys.index('static int\n'+name+' SYSCTL_HANDLER_ARGS')
    return keys[start:keys.index('\nSYSCTL_PROC',start)]+'\n'

lut_start = keys.rfind('static const struct', 0, keys.index('} jg30_ocv_lut[]'))
lut_end = keys.index('\nstatic unsigned\njg30_estimate_soc', lut_start)
snapshot_header = (source/'bsd/dev/river_battery.h').read_text()
snapshot_code = battery_pre+snapshot_header+keys[lut_start:lut_end]+function(keys,'jg30_estimate_soc')+function(keys,'river_battery_observe')+battery_model+function(battery,'SweetBattery::pollRiver')+battery_main
backlight_code = backlight_pre+''.join(function(fb,n) for n in ['AppleCLCD::setPanelLevel','AppleCLCD::setProperties','AppleCLCD::blWork'])+backlight_main
hook_code = hook_pre+bridge[bridge.index('static void (*screen_original)'):bridge.index('#ifndef RIVER_HARDWARE_TEST')]+hook_main
haptics_code=haptics_pre+function(keys,'river_haptic_play')+function(keys,'river_vibrate')+sysctl_function('river_vibrate_sysctl')+sysctl_function('river_haptic_sysctl')+haptics_main

with tempfile.TemporaryDirectory(prefix='river-hardware-') as tmp:
    tmp=Path(tmp)
    for name,code,flags,suffix in [('battery',snapshot_code,[],'cpp'),('backlight',backlight_code,[],'cpp'),('haptics',haptics_code,[],'c'),('hooks',hook_code,['-fobjc-arc','-framework','Foundation'],'m')]:
        src=tmp/(name+'.'+suffix); binary=tmp/name;src.write_text(code)
        cmd=['clang++','-std=c++17'] if suffix=='cpp' else ['clang']
        subprocess.run(cmd+['-O1','-g','-fsanitize=address,undefined','-Wno-unused-parameter',str(src),'-o',str(binary)]+flags,check=True)
        subprocess.run([str(binary)],check=True)

    policy=tmp/'policy.c';binary=tmp/'policy'
    policy.write_text(r'''
#include "river-hardware-policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(river_brightness_fixed(0)==0&&river_brightness_fixed(1)==(500u<<16));
 assert(river_brightness_fixed(2)==(500u<<16)&&river_brightness_fixed(-1)==0);
 assert(river_brightness_fixed(.5)==(250u<<16));
 struct river_feedback_pattern p;
 assert(!river_feedback_pattern(0,NAN,&p)&&!river_feedback_pattern(0,INFINITY,&p));
 assert(!river_feedback_pattern(99,1,&p)&&!river_feedback_pattern(0,0,&p));
 for(unsigned k=0;k<5;k++)for(unsigned i=1;i<=200;i++){
  if(!river_feedback_pattern(k,(double)i/100,&p))continue;
  assert(p.count>0&&p.count<=3);
  for(unsigned j=0;j<p.count;j++)assert(p.pulses[j].gain>0&&p.pulses[j].gain<=0x28&&p.pulses[j].duration_ms<=1000);
 }
 puts("PASS: bridge brightness units/range and bounded feedback patterns, invalid/NaN/infinite/zero inputs");
}''')
    subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(repo/'phone-tools'),str(policy),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
