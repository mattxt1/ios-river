/* iOS API capture test. Success means a decodable PCM file of the requested
 * length. Physical microphone capture still requires an external sound test. */
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include <math.h>
#include <errno.h>
#include <time.h>

static double monotonic_seconds(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

@interface RiverRecordingResult : NSObject <AVAudioRecorderDelegate>
@property BOOL finished;
@property BOOL succeeded;
@end
@implementation RiverRecordingResult
- (void)audioRecorderDidFinishRecording:(AVAudioRecorder *)recorder successfully:(BOOL)flag {
    (void)recorder; self.succeeded = flag; self.finished = YES;
}
- (void)audioRecorderEncodeErrorDidOccur:(AVAudioRecorder *)recorder error:(NSError *)error {
    (void)recorder; NSLog(@"Capture error: %@", error); self.succeeded = NO; self.finished = YES;
}
@end

int main(int argc, char **argv) {
    @autoreleasepool {
        char *end = NULL;
        double duration = argc > 1 ? strtod(argv[1], &end) : 2;
        if (argc > 3 || (argc > 1 && (!*argv[1] || *end)) || !isfinite(duration) || duration < 0.1 || duration > 60) {
            fprintf(stderr, "Usage: %s [seconds 0.1..60] [new.caf]\n", argv[0]); return 2;
        }
        NSString *path = argc > 2 ? [NSString stringWithUTF8String:argv[2]] : @"/tmp/record_test.caf";
        if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
            NSLog(@"Refusing to overwrite %@; choose a new capture path", path); return 1;
        }
        AVAudioSession *session = [AVAudioSession sharedInstance];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        AVAudioSessionRecordPermission permission = session.recordPermission;
#pragma clang diagnostic pop
        if (permission != AVAudioSessionRecordPermissionGranted) {
            NSLog(@"Microphone permission is not granted for this command-line process (%#lx). "
                   "Test recording from an authorized iOS app; no capture was started.", (unsigned long)permission);
            return 1;
        }
        NSError *error = nil;
        if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
                     withOptions:AVAudioSessionCategoryOptionDefaultToSpeaker error:&error] ||
            ![session setPreferredSampleRate:48000 error:&error] ||
            ![session setActive:YES error:&error]) {
            NSLog(@"Audio session failed: %@", error); return 1;
        }
        int result = 1;
        AVAudioRecorder *recorder = nil;
        if (!session.inputAvailable) { NSLog(@"No iOS audio input available"); goto cleanup; }
        NSLog(@"iOS route: %@, rate %.0f Hz. Verify this capture against a changing external sound.",
              session.currentRoute, session.sampleRate);
        recorder = [[AVAudioRecorder alloc] initWithURL:[NSURL fileURLWithPath:path] settings:@{
            AVFormatIDKey: @(kAudioFormatLinearPCM), AVSampleRateKey: @48000,
            AVNumberOfChannelsKey: @1, AVLinearPCMBitDepthKey: @16,
            AVLinearPCMIsBigEndianKey: @NO, AVLinearPCMIsFloatKey: @NO
        } error:&error];
        if (!recorder) { NSLog(@"Recorder creation failed: %@", error); goto cleanup; }
        {
            RiverRecordingResult *state = [RiverRecordingResult new];
            recorder.delegate = state;
            if (![recorder prepareToRecord] || ![recorder record]) {
                NSLog(@"Recorder refused capture"); goto cleanup;
            }
            double deadline = monotonic_seconds() + duration;
            while (!state.finished && monotonic_seconds() < deadline)
                [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
            NSLog(@"Stopping capture: recording=%d recorderTime=%.3f s", recorder.recording, recorder.currentTime);
            [recorder stop];
            // stop closes the file. Decode it even when CLI delegate delivery
            // is absent; a file of the correct duration is the test criterion.
            deadline = monotonic_seconds() + 0.25;
            while (!state.finished && monotonic_seconds() < deadline)
                [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
            if (state.finished && !state.succeeded) { NSLog(@"Capture reported an encoding failure"); goto cleanup; }
        }
        {
            AVAudioFile *file = [[AVAudioFile alloc] initForReading:[NSURL fileURLWithPath:path] error:&error];
            if (!file) { NSLog(@"Captured file cannot be decoded: %@", error); goto cleanup; }
            double actual = (double)file.length / file.processingFormat.sampleRate;
            NSLog(@"Captured %lld PCM frames, %.3f s, %@", (long long)file.length, actual, file.fileFormat);
            if (file.fileFormat.channelCount != 1 || fabs(file.fileFormat.sampleRate - 48000) > 1 ||
                actual < duration - 0.1 || actual > duration + 0.25 || !file.length) {
                NSLog(@"Capture length/format mismatch"); goto cleanup;
            }
            result = 0;
        }
cleanup:
        [recorder stop];
        if (result && recorder) [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
        [session setActive:NO error:NULL];
        return result;
    }
}
