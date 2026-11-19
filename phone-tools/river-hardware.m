/* Opt-in native iOS -> River hardware bridge. This is a pulse backend for
 * UIKit feedback, not a replacement Core Haptics engine. See HARDWARE.md. */
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <IOKit/IOKitLib.h>
#include <dlfcn.h>
#include <math.h>
#include <objc/runtime.h>
#include <sys/sysctl.h>
#include "river-hardware-policy.h"

static dispatch_queue_t hardware_queue;
static dispatch_source_t brightness_timer;
static io_service_t framebuffer;
static double pending_brightness;
static BOOL brightness_pending, brightness_enabled, feedback_enabled;
static uint64_t feedback_generation;
static _Thread_local unsigned feedback_depth;

static BOOL enabled(const char *key) {
 const char *value = getenv(key);
 return value && !strcmp(value, "1");
}

static BOOL feedback_allowed(void) {
 if (!feedback_enabled) return NO;
 /* Explicit user disable wins. UIKit's original method still runs so native
  * lifecycle/policy work isn't suppressed. This preference is provisional
  * until the actual iOS16 Settings path is traced on River. */
 CFPropertyListRef pref = CFPreferencesCopyAppValue(CFSTR("haptic-feedback"), CFSTR("com.apple.preferences.sounds"));
 BOOL allowed = !pref || ![(__bridge id)pref respondsToSelector:@selector(boolValue)] || [(__bridge id)pref boolValue];
 if (pref) CFRelease(pref);
 return allowed;
}

static BOOL feedback_ready(void) {
 int ready = 0; size_t size = sizeof(ready);
 return sysctlbyname("debug.river_haptic", &ready, &size, NULL, 0) == 0 && size == sizeof(ready) && ready == 1;
}

static void send_pulse(unsigned ms, unsigned gain) {
 int packet = (int)((gain << 16) | ms);
 /* No privileged register access here. Kernel validates every request and
  * performs I2C on its existing worker. Failure is not playback success. */
 if (sysctlbyname("debug.river_haptic", NULL, NULL, &packet, sizeof(packet)) != 0)
  NSLog(@"River feedback request rejected: %s", strerror(errno));
}

static void feedback(unsigned kind, double intensity) {
 struct river_feedback_pattern pattern;
 if (!feedback_allowed() || !river_feedback_pattern(kind, intensity, &pattern)) return;
 dispatch_async(hardware_queue, ^{
  uint64_t generation = ++feedback_generation;
  if (!feedback_ready()) return;
  for (unsigned i = 0; i < pattern.count; i++) {
   struct river_feedback_pulse pulse = pattern.pulses[i];
   dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)pulse.offset_ms * NSEC_PER_MSEC), hardware_queue, ^{
    if (generation == feedback_generation && feedback_allowed()) send_pulse(pulse.duration_ms, pulse.gain);
   });
  }
 });
}

static void flush_brightness(void) {
 if (!brightness_pending) return;
 brightness_pending = NO;
 if (!framebuffer) {
  framebuffer = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleCLCD"));
  if (!framebuffer) return;
  CFTypeRef supported = IORegistryEntryCreateCFProperty(framebuffer, CFSTR("RiverHardwareBacklight"), kCFAllocatorDefault, 0);
  BOOL valid = supported && CFEqual(supported, kCFBooleanTrue);
  if (supported) CFRelease(supported);
  if (!valid) { IOObjectRelease(framebuffer); framebuffer = 0; return; }
 }
 uint64_t fixed = river_brightness_fixed(pending_brightness);
 NSDictionary *props = @{ @"IOMFBBrightnessLevel": @(fixed) };
 typedef kern_return_t (*set_properties_fn)(io_registry_entry_t, CFTypeRef);
 set_properties_fn set_properties = (set_properties_fn)dlsym(RTLD_DEFAULT, "IORegistryEntrySetCFProperties");
 kern_return_t kr = set_properties ? set_properties(framebuffer, (__bridge CFTypeRef)props) : kIOReturnUnsupported;
 if (kr != KERN_SUCCESS) {
  NSLog(@"River backlight request rejected: %#x", kr);
  IOObjectRelease(framebuffer); framebuffer = 0;
 }
}

static void brightness(double value) {
 if (!brightness_enabled || !isfinite(value)) return;
 dispatch_async(hardware_queue, ^{
  pending_brightness = value;
  if (brightness_pending) return; /* bounded latest-value coalescing */
  brightness_pending = YES;
  dispatch_source_set_timer(brightness_timer, dispatch_time(DISPATCH_TIME_NOW, 16 * NSEC_PER_MSEC), DISPATCH_TIME_FOREVER, NSEC_PER_MSEC);
 });
}

static void (*screen_original)(UIScreen *, SEL, CGFloat);
static void screen_brightness(UIScreen *self, SEL cmd, CGFloat value) {
 screen_original(self, cmd, value);
 if (self == UIScreen.mainScreen) brightness(value);
}
static void (*selection_original)(id, SEL);
static void selection(id self, SEL cmd) {
 BOOL outer = feedback_depth++ == 0;
 @try { selection_original(self, cmd); }
 @finally { --feedback_depth; }
 if (outer) feedback(RIVER_FEEDBACK_SELECTION, 1);
}
static void (*impact_original)(id, SEL);
static void impact(id self, SEL cmd) {
 BOOL outer = feedback_depth++ == 0;
 @try { impact_original(self, cmd); }
 @finally { --feedback_depth; }
 if (outer) feedback(RIVER_FEEDBACK_IMPACT, 1);
}
static void (*intensity_original)(id, SEL, CGFloat);
static void impact_intensity(id self, SEL cmd, CGFloat value) {
 BOOL outer = feedback_depth++ == 0;
 @try { intensity_original(self, cmd, value); }
 @finally { --feedback_depth; }
 if (outer) feedback(RIVER_FEEDBACK_IMPACT, value);
}
static void (*notification_original)(id, SEL, UINotificationFeedbackType);
static void notification(id self, SEL cmd, UINotificationFeedbackType type) {
 BOOL outer = feedback_depth++ == 0;
 @try { notification_original(self, cmd, type); }
 @finally { --feedback_depth; }
 if (outer && type >= UINotificationFeedbackTypeSuccess && type <= UINotificationFeedbackTypeError)
  feedback(RIVER_FEEDBACK_SUCCESS + (unsigned)type, 1);
}

/* Verify the actual method ABI before installing, and add a local method
 * instead of replacing an inherited implementation on a parent class. */
static BOOL hook(Class cls, SEL sel, IMP replacement, IMP *original, const char *argument) {
 if (!cls || *original) return NO;
 Method method = class_getInstanceMethod(cls, sel);
 if (!method) return NO;
 char result[16], arg[16];
 method_getReturnType(method, result, sizeof(result));
 if (strcmp(result, "v") || method_getNumberOfArguments(method) != (argument ? 3u : 2u)) return NO;
 if (argument) {
  method_getArgumentType(method, 2, arg, sizeof(arg));
  if (strcmp(arg, argument)) return NO;
 }
 *original = method_getImplementation(method);
 if (!class_addMethod(cls, sel, replacement, method_getTypeEncoding(method))) method_setImplementation(method, replacement);
 return YES;
}

#ifndef RIVER_HARDWARE_TEST
/* BackBoardServices setter used by brightness clients. It remains weak so
 * this library also loads in processes without that private framework.
 * The public UIScreen observer is the additional native entry point. */
extern void BKSDisplayBrightnessSet(float, int) __attribute__((weak_import));
static void bridge_bks_brightness(float value, int options) {
 /* dyld does not interpose a dylib's own reference to its original. */
 if (BKSDisplayBrightnessSet) BKSDisplayBrightnessSet(value, options);
 brightness(value);
}
__attribute__((used, section("__DATA,__interpose"))) static const struct {
 const void *replacement, *original;
} interposers[] = {{ (const void *)bridge_bks_brightness, (const void *)BKSDisplayBrightnessSet }};

__attribute__((constructor)) static void river_hardware_loaded(void) {
 brightness_enabled = enabled("RIVER_HW_BRIGHTNESS");
 feedback_enabled = enabled("RIVER_HW_FEEDBACK");
 if (!brightness_enabled && !feedback_enabled) return;
 hardware_queue = dispatch_queue_create("org.ios-river.hardware", DISPATCH_QUEUE_SERIAL);
 brightness_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, hardware_queue);
 dispatch_source_set_timer(brightness_timer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 0);
 dispatch_source_set_event_handler(brightness_timer, ^{ flush_brightness(); });
 dispatch_resume(brightness_timer);
 /* Inject only into selected UIKit processes; this library links UIKit. */
 dispatch_async(dispatch_get_main_queue(), ^{
  if (brightness_enabled) {
   hook(UIScreen.class, @selector(setBrightness:), (IMP)screen_brightness, (IMP *)&screen_original, @encode(CGFloat));
   [NSNotificationCenter.defaultCenter addObserverForName:UIScreenBrightnessDidChangeNotification object:UIScreen.mainScreen queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *n) { brightness(UIScreen.mainScreen.brightness); }];
  }
  if (feedback_enabled) {
   hook(UISelectionFeedbackGenerator.class, @selector(selectionChanged), (IMP)selection, (IMP *)&selection_original, NULL);
   hook(UIImpactFeedbackGenerator.class, @selector(impactOccurred), (IMP)impact, (IMP *)&impact_original, NULL);
   hook(UIImpactFeedbackGenerator.class, @selector(impactOccurredWithIntensity:), (IMP)impact_intensity, (IMP *)&intensity_original, @encode(CGFloat));
   hook(UINotificationFeedbackGenerator.class, @selector(notificationOccurred:), (IMP)notification, (IMP *)&notification_original, @encode(UINotificationFeedbackType));
  }
 });
}
#endif
