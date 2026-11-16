#include <string.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>

int main(int argc, char **argv) {
    @autoreleasepool {
        const char *path = argc > 1 ? argv[1] : "/System/Library/Audio/UISounds/connect_power.caf";
        NSString *filePath = [NSString stringWithUTF8String:path];
        NSURL *fileURL = [NSURL fileURLWithPath:filePath];

        NSLog(@"Setting up audio session...");
        AVAudioSession *session = [AVAudioSession sharedInstance];
        NSError *err = nil;
        if (![session setCategory:AVAudioSessionCategoryPlayback error:&err] ||
            ![session setActive:YES error:&err]) {
            NSLog(@"Audio session failed: %@", err); return 1;
        }

        NSLog(@"Loading sound from %@...", filePath);
        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:fileURL error:&err];
        if (err || !player) {
            NSLog(@"Failed to init AVAudioPlayer: %@", err);
            return 1;
        }

        player.volume = 0.1f;
        if (argc > 2 && strcmp(argv[2], "--listening-window") == 0) player.numberOfLoops = 3;
        NSLog(@"Playing sound (duration %.2f s)...", player.duration);
        BOOL ok = [player play];
        NSLog(@"play returned: %d; physical sound must be checked acoustically", ok);
        if (!ok) { [session setActive:NO error:NULL]; return 1; }

        // Sleep while playing
        NSTimeInterval dur = player.duration > 0 ? player.duration * (player.numberOfLoops + 1) + 0.5 : 2.0;
        [NSThread sleepForTimeInterval:dur];
        NSLog(@"Done.");
        [session setActive:NO error:NULL];
    }
    return 0;
}
