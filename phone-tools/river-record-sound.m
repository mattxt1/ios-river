#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

int main(int argc, char **argv) {
    @autoreleasepool {
        float duration = argc > 1 ? atof(argv[1]) : 2.0f;
        NSString *outPath = @"/tmp/record_test.caf";
        NSURL *outURL = [NSURL fileURLWithPath:outPath];

        [[NSFileManager defaultManager] removeItemAtPath:outPath error:nil];

        NSLog(@"Setting up AVAudioSession for PlayAndRecord...");
        AVAudioSession *session = [AVAudioSession sharedInstance];
        NSError *err = nil;
        [session setCategory:AVAudioSessionCategoryPlayAndRecord error:&err];
        if (err) NSLog(@"setCategory error: %@", err);
        [session setActive:YES error:&err];
        if (err) NSLog(@"setActive error: %@", err);

        NSDictionary *settings = @{
            AVFormatIDKey: @(kAudioFormatLinearPCM),
            AVSampleRateKey: @(48000.0f),
            AVNumberOfChannelsKey: @(1),
            AVLinearPCMBitDepthKey: @(16),
            AVLinearPCMIsBigEndianKey: @(NO),
            AVLinearPCMIsFloatKey: @(NO)
        };

        NSLog(@"Initializing AVAudioRecorder...");
        AVAudioRecorder *recorder = [[AVAudioRecorder alloc] initWithURL:outURL settings:settings error:&err];
        if (err || !recorder) {
            NSLog(@"Failed to init AVAudioRecorder: %@", err);
            return 1;
        }

        NSLog(@"Recording for %.1f s...", duration);
        BOOL ok = [recorder recordForDuration:duration];
        NSLog(@"recordForDuration returned: %d", ok);

        [NSThread sleepForTimeInterval:duration + 0.5];
        [recorder stop];
        NSLog(@"Recording stopped.");

        NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:outPath error:&err];
        if (attrs) {
            NSLog(@"Output file size: %llu bytes", [attrs fileSize]);
        } else {
            NSLog(@"Output file not found: %@", err);
        }
    }
    return 0;
}
