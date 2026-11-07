#import <Foundation/Foundation.h>
#include <dlfcn.h>

@interface LSApplicationWorkspace : NSObject
+ (instancetype)defaultWorkspace;
- (BOOL)openSensitiveURL:(NSURL *)url withOptions:(NSDictionary *)options;
- (BOOL)openURL:(NSURL *)url;
@end

int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc != 2) {
            fprintf(stderr, "usage: openurl URL\n");
            return 2;
        }
        dlopen("/System/Library/Frameworks/CoreServices.framework/CoreServices", RTLD_NOW);
        id ws = [NSClassFromString(@"LSApplicationWorkspace") defaultWorkspace];
        NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:argv[1]]];
        BOOL ok = NO;
        if ([ws respondsToSelector:@selector(openSensitiveURL:withOptions:)]) {
            ok = [ws openSensitiveURL:url withOptions:nil];
        } else if ([ws respondsToSelector:@selector(openURL:)]) {
            ok = [ws openURL:url];
        }
        printf("open %s: %s\n", argv[1], ok ? "yes" : "NO");
        return ok ? 0 : 1;
    }
}
