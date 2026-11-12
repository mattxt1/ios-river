/* Foreground iOS audio diagnostic. Microphone access uses the normal consent
 * prompt. A file or playback callback alone does not prove physical audio. */
#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#include <math.h>

@interface RiverAudioCheck : UIViewController <AVAudioRecorderDelegate, AVAudioPlayerDelegate>
@property UILabel *status;
@property UILabel *route;
@property NSArray<UIButton *> *buttons;
@property AVAudioRecorder *recorder;
@property AVAudioPlayer *player;
@property NSTimer *timer;
@property NSURL *pendingClip;
@property NSURL *lastClip;
@end
@implementation RiverAudioCheck
- (void)viewDidLoad {
    [super viewDidLoad]; self.view.backgroundColor = UIColor.systemBackgroundColor;
    UILabel *title = [UILabel new]; title.text = @"River Audio Check"; title.font = [UIFont boldSystemFontOfSize:26];
    UILabel *help = [UILabel new]; help.text = @"Test the speaker and record a short clip. Confirm output by listening; confirm input with a changing external sound.";
    help.numberOfLines = 0;
    self.route = [UILabel new]; self.route.numberOfLines = 0; self.route.font = [UIFont systemFontOfSize:14];
    self.status = [UILabel new]; self.status.numberOfLines = 0; self.status.text = @"Ready. Physical audio is still under development.";
    NSMutableArray *buttons = [NSMutableArray new];
    NSArray *titles = @[@"Play quiet test tone", @"Record 3 seconds", @"Play last recording"];
    NSArray *actions = @[@"tone", @"record", @"replay"];
    for (NSUInteger i = 0; i < titles.count; i++) {
        UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
        [b setTitle:titles[i] forState:UIControlStateNormal]; b.titleLabel.font = [UIFont systemFontOfSize:18];
        [b addTarget:self action:NSSelectorFromString(actions[i]) forControlEvents:UIControlEventTouchUpInside];
        [b.heightAnchor constraintEqualToConstant:52].active = YES; [buttons addObject:b];
    }
    self.buttons = buttons;
    UIStackView *stack = [[UIStackView alloc] initWithArrangedSubviews:@[title,help,self.route,buttons[0],buttons[1],buttons[2],self.status]];
    stack.axis = UILayoutConstraintAxisVertical; stack.spacing = 16; stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:stack]; UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[stack.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:24],
        [stack.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-24],
        [stack.topAnchor constraintEqualToAnchor:safe.topAnchor constant:24]]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(interrupted:) name:AVAudioSessionInterruptionNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(background:) name:UIApplicationDidEnterBackgroundNotification object:nil];
    [self updateRoute];
}
- (void)dealloc { [[NSNotificationCenter defaultCenter] removeObserver:self]; }
- (void)busy:(BOOL)busy { for (UIButton *b in self.buttons) b.enabled = !busy; }
- (void)updateRoute {
    AVAudioSession *s = AVAudioSession.sharedInstance;
    self.route.text = [NSString stringWithFormat:@"Input: %@\nOutput: %@\nSession: %.0f Hz",
        s.currentRoute.inputs.firstObject.portName ?: @"None", s.currentRoute.outputs.firstObject.portName ?: @"None", s.sampleRate];
}
- (BOOL)activate {
    NSError *error = nil; AVAudioSession *s = AVAudioSession.sharedInstance;
    if (![s setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:AVAudioSessionCategoryOptionDefaultToSpeaker error:&error] ||
        ![s setPreferredSampleRate:48000 error:&error] || ![s setActive:YES error:&error]) {
        self.status.text = error.localizedDescription ?: @"Audio session failed."; return NO;
    }
    [self updateRoute]; return YES;
}
- (NSURL *)newFile:(NSString *)extension {
    NSURL *directory = [[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    NSError *error = nil;
    if (!directory || ![[NSFileManager defaultManager] createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]) {
        self.status.text = error.localizedDescription ?: @"Recording directory unavailable."; return nil;
    }
    return [directory URLByAppendingPathComponent:[NSUUID.UUID.UUIDString stringByAppendingPathExtension:extension]];
}
- (void)playURL:(NSURL *)url {
    if (![self activate]) return;
    NSError *error = nil; self.player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
    self.player.delegate = self; self.player.volume = 0.2;
    if (!self.player || ![self.player play]) { self.status.text = error.localizedDescription ?: @"Playback could not start."; return; }
    [self busy:YES]; self.status.text = @"Playing. Listen for the sound.";
}
- (void)tone {
    NSURL *url = [self newFile:@"wav"]; if (!url) return;
    // One second, 48 kHz stereo, little-endian PCM16, -26 dBFS.
    NSMutableData *data = [NSMutableData dataWithLength:44 + 48000 * 4]; uint8_t *p = data.mutableBytes;
    memcpy(p,"RIFF",4); uint32_t n = CFSwapInt32HostToLittle(36 + 48000*4); memcpy(p+4,&n,4);
    memcpy(p+8,"WAVEfmt ",8); n=CFSwapInt32HostToLittle(16); memcpy(p+16,&n,4);
    uint16_t w=CFSwapInt16HostToLittle(1); memcpy(p+20,&w,2); w=CFSwapInt16HostToLittle(2); memcpy(p+22,&w,2);
    n=CFSwapInt32HostToLittle(48000); memcpy(p+24,&n,4); n=CFSwapInt32HostToLittle(192000); memcpy(p+28,&n,4);
    w=CFSwapInt16HostToLittle(4); memcpy(p+32,&w,2); w=CFSwapInt16HostToLittle(16); memcpy(p+34,&w,2);
    memcpy(p+36,"data",4); n=CFSwapInt32HostToLittle(48000*4); memcpy(p+40,&n,4);
    int16_t *samples=(int16_t *)(p+44);
    for (int i=0;i<48000;i++) samples[2*i]=samples[2*i+1]=(int16_t)(sin(2*M_PI*440*i/48000)*1638);
    NSError *error=nil;
    if (![data writeToURL:url options:NSDataWritingWithoutOverwriting error:&error]) { self.status.text=error.localizedDescription; return; }
    [self playURL:url];
}
- (void)record {
    [self busy:YES];
    [AVAudioSession.sharedInstance requestRecordPermission:^(BOOL granted) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!granted) { self.status.text=@"Microphone permission was denied. Allow it in Settings to record."; [self busy:NO]; return; }
            if (UIApplication.sharedApplication.applicationState != UIApplicationStateActive) { [self busy:NO]; return; }
            [self beginRecording];
        });
    }];
}
- (void)beginRecording {
    if (![self activate]) { [self busy:NO]; return; }
    self.pendingClip=[self newFile:@"caf"]; if (!self.pendingClip) { [self busy:NO]; return; }
    NSError *error=nil;
    self.recorder=[[AVAudioRecorder alloc] initWithURL:self.pendingClip settings:@{
        AVFormatIDKey:@(kAudioFormatLinearPCM),AVSampleRateKey:@48000,AVNumberOfChannelsKey:@1,
        AVLinearPCMBitDepthKey:@16,AVLinearPCMIsFloatKey:@NO,AVLinearPCMIsBigEndianKey:@NO} error:&error];
    self.recorder.delegate=self;
    if (!self.recorder || ![self.recorder prepareToRecord] || ![self.recorder record]) {
        self.status.text=error.localizedDescription ?: @"Recording did not start."; [self cancelCapture]; return;
    }
    self.status.text=@"Recording for 3 seconds. Speak or clap near the microphone.";
    self.timer=[NSTimer scheduledTimerWithTimeInterval:3 target:self selector:@selector(finishRecording) userInfo:nil repeats:NO];
}
- (void)finishRecording {
    [self.timer invalidate]; self.timer=nil; [self.recorder stop]; self.recorder=nil;
    NSError *error=nil; AVAudioFile *file=[[AVAudioFile alloc] initForReading:self.pendingClip error:&error];
    double seconds=file ? (double)file.length/file.processingFormat.sampleRate : 0;
    if (!file || file.length == 0 || seconds < 2.75 || seconds > 3.5) {
        self.status.text=[NSString stringWithFormat:@"Capture incomplete: %lld frames, %.3f seconds. %@",(long long)file.length,seconds,error.localizedDescription ?: @""];
        [[NSFileManager defaultManager] removeItemAtURL:self.pendingClip error:NULL];
    } else {
        self.lastClip=self.pendingClip;
        self.status.text=[NSString stringWithFormat:@"Recorded %lld frames (%.3f seconds). Play the recording and check that it matches your sound.",(long long)file.length,seconds];
    }
    self.pendingClip=nil; [self busy:NO]; [AVAudioSession.sharedInstance setActive:NO error:NULL];
}
- (void)replay { if (self.lastClip) [self playURL:self.lastClip]; else self.status.text=@"Record a clip first."; }
- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer *)player successfully:(BOOL)flag {
    (void)player; self.status.text=flag ? @"Playback finished. Confirm whether you heard the sound." : @"Playback failed.";
    [self busy:NO]; [AVAudioSession.sharedInstance setActive:NO error:NULL];
}
- (void)audioRecorderEncodeErrorDidOccur:(AVAudioRecorder *)recorder error:(NSError *)error {
    (void)recorder; [self cancelCapture]; self.status.text=error.localizedDescription;
}
- (void)cancelCapture {
    [self.timer invalidate]; self.timer=nil; [self.recorder stop]; self.recorder=nil;
    if (self.pendingClip) [[NSFileManager defaultManager] removeItemAtURL:self.pendingClip error:NULL];
    self.pendingClip=nil; [self.player stop]; [self busy:NO]; [AVAudioSession.sharedInstance setActive:NO error:NULL];
}
- (void)background:(NSNotification *)notification { (void)notification; [self cancelCapture]; self.status.text=@"Audio test stopped when the app left the foreground."; }
- (void)interrupted:(NSNotification *)notification {
    if ([notification.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue] == AVAudioSessionInterruptionTypeBegan) {
        [self cancelCapture]; self.status.text=@"Audio test interrupted.";
    }
}
@end
@interface RiverAudioApp : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow *window;
@end
@implementation RiverAudioApp
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)application; (void)options;
    self.window=[[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds]; self.window.rootViewController=[RiverAudioCheck new];
    [self.window makeKeyAndVisible]; return YES;
}
@end
int main(int argc,char **argv) { @autoreleasepool { return UIApplicationMain(argc,argv,nil,NSStringFromClass(RiverAudioApp.class)); } }
