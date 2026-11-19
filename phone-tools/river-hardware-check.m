/* Default is read-only. Explicit feedback/brightness arguments exercise the
 * native API in this process; use the standalone bridge for controlled tests. */
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreHaptics/CoreHaptics.h>
#include <IOKit/IOKitLib.h>
#include <objc/runtime.h>
#include <sys/sysctl.h>
#include <math.h>

static id sysctl_value(const char *name) {
 size_t size=0;
 if(sysctlbyname(name,NULL,&size,NULL,0)!=0 || size>4096)return NSNull.null;
 NSMutableData *data=[NSMutableData dataWithLength:size];
 if(sysctlbyname(name,data.mutableBytes,&size,NULL,0)!=0)return NSNull.null;
 if(!strcmp(name,"debug.river_haptic") || !strcmp(name,"debug.river_native_feedback")){
  int value=0;if(size!=sizeof(value))return NSNull.null;memcpy(&value,data.bytes,sizeof(value));return @(value);
 }
 if(!size)return @"";
 const char *bytes=data.bytes;
 return [[NSString alloc]initWithBytes:bytes length:(bytes[size-1]?size:size-1) encoding:NSUTF8StringEncoding]?:NSNull.null;
}

static NSDictionary *properties(const char *name,NSArray *wanted){
 io_service_t service=IOServiceGetMatchingService(kIOMainPortDefault,IOServiceMatching(name));
 if(!service)return @{@"present":@NO};
 NSMutableDictionary *out=[@{@"present":@YES} mutableCopy];
 for(NSString *key in wanted){
  CFTypeRef value=IORegistryEntryCreateCFProperty(service,(__bridge CFStringRef)key,kCFAllocatorDefault,0);
  if(value)out[key]=CFBridgingRelease(value);
 }
 IOObjectRelease(service);return out;
}

static NSArray *methods(NSString *name){
 Class cls=NSClassFromString(name);if(!cls)return @[];
 unsigned count=0;Method *list=class_copyMethodList(cls,&count);NSMutableArray *out=[NSMutableArray new];
 for(unsigned i=0;i<count && out.count<128;i++){
  NSString *sel=NSStringFromSelector(method_getName(list[i]));
  if([sel rangeOfString:@"play" options:NSCaseInsensitiveSearch].location!=NSNotFound ||
     [sel rangeOfString:@"feedback" options:NSCaseInsensitiveSearch].location!=NSNotFound)
   [out addObject:@{@"selector":sel,@"encoding":@(method_getTypeEncoding(list[i]))}];
 }
 free(list);return out;
}

int main(int argc,char **argv){@autoreleasepool{
 if(argc!=1 && !(argc==3 && (!strcmp(argv[1],"--feedback") || !strcmp(argv[1],"--brightness")))){
  fprintf(stderr,"usage: river-hardware-check [--feedback selection|impact|success|warning|error | --brightness 0..1]\n");return 2;
 }
 if(argc==3){
  /* Give the injected library's main-queue installation a chance to run. */
  [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.1]];
  if(!strcmp(argv[1],"--brightness")){
   char *end=NULL;double value=strtod(argv[2],&end);
   if(!end || end==argv[2] || *end || !isfinite(value) || value<0 || value>1)return 2;
   UIScreen.mainScreen.brightness=value;
  }else{
   NSString *kind=@(argv[2]);
   if([kind isEqual:@"selection"]){UISelectionFeedbackGenerator *g=[UISelectionFeedbackGenerator new];[g prepare];[g selectionChanged];}
   else if([kind isEqual:@"impact"]){UIImpactFeedbackGenerator *g=[[UIImpactFeedbackGenerator alloc]initWithStyle:UIImpactFeedbackStyleMedium];[g prepare];[g impactOccurred];}
   else{
    NSInteger type=[@[@"success",@"warning",@"error"] indexOfObject:kind];if(type==NSNotFound)return 2;
    UINotificationFeedbackGenerator *g=[UINotificationFeedbackGenerator new];[g prepare];[g notificationOccurred:(UINotificationFeedbackType)type];
   }
  }
  [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.5]];
 }
 id<CHHapticDeviceCapability> cap=[CHHapticEngine capabilitiesForHardware];
 NSDictionary *report=@{
  @"native_mode":sysctl_value("debug.river_native_feedback"),
  @"haptic_ready":sysctl_value("debug.river_haptic"),
  @"battery_sample":sysctl_value("debug.river_battery"),
  @"battery":properties("SweetBattery",@[@"SweetBatteryPercent",@"Voltage",@"Amperage",@"ExternalConnected",@"ExternalChargeCapable",@"IsCharging",@"FullyCharged",@"SweetBatterySampleAvailable",@"SweetBatterySampleStale",@"RiverChargerState",@"RiverChargerError",@"RiverChargerTemperatureStatus",@"RiverChargerModes",@"RiverChargerCommand",@"RiverPowerPath"]),
  @"display":properties("AppleCLCD",@[@"RiverHardwareBacklight",@"RiverBacklightMaxNits",@"IOMFBBrightnessLevel",@"IOMFBLinearBrightnessLevel"]),
  @"core_haptics":@{@"supports_haptics":@(cap.supportsHaptics),@"supports_audio":@(cap.supportsAudio),@"note":@"Framework capability is not physical playback evidence"},
  @"feedback_engine_methods":methods(@"_UIFeedbackEngine"),
  @"feedback_generator_methods":methods(@"_UIFeedbackGenerator"),
  @"action_requested":argc==3?@(argv[1]):@"none"
 };
 NSError *error=nil;NSData *json=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];
 if(!json){fprintf(stderr,"JSON error: %s\n",error.description.UTF8String);return 1;}
 fwrite(json.bytes,1,json.length,stdout);puts("");return 0;
}}
