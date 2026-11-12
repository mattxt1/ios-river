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
        [session setCategory:AVAudioSessionCategoryPlayback error:&err];
        if (err) NSLog(@"setCategory error: %@", err);
        [session setActive:YES error:&err];
        if (err) NSLog(@"setActive error: %@", err);

        NSLog(@"Loading sound from %@...", filePath);
        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:fileURL error:&err];
        if (err || !player) {
            NSLog(@"Failed to init AVAudioPlayer: %@", err);
            return 1;
        }

        player.volume = 1.0f;
        NSLog(@"Playing sound (duration %.2f s)...", player.duration);
        BOOL ok = [player play];
        NSLog(@"play returned: %d", ok);

        // Sleep while playing
        NSTimeInterval dur = player.duration > 0 ? player.duration + 0.5 : 2.0;
        [NSThread sleepForTimeInterval:dur];
        NSLog(@"Done.");
    }
    return 0;
}
