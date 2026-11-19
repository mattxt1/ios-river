#!/usr/bin/env python3
"""Run actual Search clipping and switcher source-cache code in an ObjC model."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parent.parent
s=(repo/'phone-tools/cgprobe.c').read_text()
clip=s[s.index('static void clip_search_capsule('):s.index('static void (*original_folder_bg_layout)')]
wall=s[s.index('static char switcher_wallpaper_key'):s.index('static void (*original_switcher_layout)')]
pre=r'''
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <assert.h>
#include <string.h>
@interface TraceLayer:NSObject
@property CGFloat cornerRadius;
@property BOOL masksToBounds;
@end
@implementation TraceLayer
@end
@interface TraceView:NSObject
@property CGRect bounds,frame;
@property BOOL hidden,clipsToBounds,userInteractionEnabled;
@property CGFloat alpha;
@property long contentMode;
@property TraceLayer *layer;
@property NSMutableArray *subviews;
@property(weak) TraceView *superview;
- (instancetype)initWithFrame:(CGRect)frame;
- (id)window;
- (void)insertSubview:(TraceView*)view atIndex:(long)index;
@end
@implementation TraceView
- (instancetype)init {if((self=[super init])){_layer=[TraceLayer new];_subviews=[NSMutableArray new];_alpha=1;}return self;}
- (instancetype)initWithFrame:(CGRect)f {if((self=[self init])){_frame=f;_bounds=CGRectMake(0,0,f.size.width,f.size.height);}return self;}
- (id)window {return [_superview window];}
- (void)insertSubview:(TraceView*)v atIndex:(long)i {[_subviews insertObject:v atIndex:i];v.superview=self;}
@end
static NSArray *allWindows;static unsigned windowScans;
@interface UIWindow:TraceView
+ (id)allWindowsIncludingInternalWindows:(BOOL)a onlyVisibleWindows:(BOOL)b;
@end
@implementation UIWindow
+ (id)allWindowsIncludingInternalWindows:(BOOL)a onlyVisibleWindows:(BOOL)b {windowScans++;return allWindows;}
- (id)window {return self;}
@end
@interface _SBWallpaperSecureWindow:UIWindow @end
@implementation _SBWallpaperSecureWindow @end
@interface PBUISnapshotReplicaView:TraceView @end
@implementation PBUISnapshotReplicaView @end
@interface UIImageView:TraceView
@property id image;
@end
@implementation UIImageView @end
@interface SBHSearchPillView:TraceView @end
@implementation SBHSearchPillView @end
@interface SBFolderScrollAccessoryView:TraceView @end
@implementation SBFolderScrollAccessoryView @end
@interface SBIconListPageControl:TraceView @end
@implementation SBIconListPageControl @end
@interface _UIPageControlContentView:TraceView @end
@implementation _UIPageControlContentView @end
'''
main=r'''
int main(void){@autoreleasepool {
 SEL layout=sel_registerName("layoutSubviews");
 SBFolderScrollAccessoryView *accessory=[SBFolderScrollAccessoryView new];
 SBHSearchPillView *pill=[[SBHSearchPillView alloc] initWithFrame:CGRectMake(0,0,78,30)];pill.hidden=YES;pill.alpha=.25;
 SBIconListPageControl *page=[SBIconListPageControl new];
 _UIPageControlContentView *content=[[_UIPageControlContentView alloc] initWithFrame:pill.bounds];content.hidden=YES;content.alpha=.3;
 [accessory insertSubview:pill atIndex:0];[accessory insertSubview:page atIndex:1];[page insertSubview:content atIndex:0];
 probe_search_pill_layout(pill,layout);
 assert(pill.layer.cornerRadius==15&&pill.layer.masksToBounds&&pill.hidden&&pill.alpha==.25);
 assert(content.layer.cornerRadius==15&&content.layer.masksToBounds&&content.hidden&&content.alpha==.3);
 assert(pill.subviews.count==0&&page.subviews.count==1&&accessory.subviews.count==2);
 pill.bounds=CGRectMake(0,0,200,40);probe_search_pill_layout(pill,layout);assert(pill.layer.cornerRadius==20);
 TraceView *other=[[TraceView alloc] initWithFrame:CGRectMake(0,0,78,30)];probe_search_pill_layout(other,layout);assert(!other.layer.masksToBounds);
 SBHSearchPillView *empty=[SBHSearchPillView new];probe_search_pill_layout(empty,layout);assert(!empty.layer.masksToBounds);
 _SBWallpaperSecureWindow *win=[_SBWallpaperSecureWindow new];PBUISnapshotReplicaView *nativeReplica=[PBUISnapshotReplicaView new];
 UIImageView *source=[UIImageView new];id a=[NSObject new],b=[NSObject new],c=[NSObject new],d=[NSObject new];source.image=a;
 [win insertSubview:nativeReplica atIndex:0];[nativeReplica insertSubview:source atIndex:0];allWindows=@[win];
 TraceView *root=[[TraceView alloc] initWithFrame:CGRectMake(0,0,375,812)];update_switcher_wallpaper(root);
 UIImageView *replica=objc_getAssociatedObject(root,&switcher_wallpaper_key);
 assert(replica.image==a&&windowScans==1&&root.subviews.count==1);
 for(int i=0;i<600;i++)update_switcher_wallpaper(root);assert(windowScans==1&&root.subviews.count==1);
 source.image=b;update_switcher_wallpaper(root);assert(replica.image==b&&windowScans==1);
 root.bounds=CGRectMake(0,0,300,600);update_switcher_wallpaper(root);assert(CGRectEqualToRect(replica.frame,root.bounds));
 [nativeReplica.subviews removeObject:source];source.superview=nil;
 UIImageView *replacement=[UIImageView new];replacement.image=c;[nativeReplica insertSubview:replacement atIndex:0];
 update_switcher_wallpaper(root);assert(replica.image==c&&windowScans==2&&objc_getAssociatedObject(root,&switcher_wallpaper_source_key)==replacement);
 replacement.image=nil;UIImageView *alternative=[UIImageView new];alternative.image=d;[nativeReplica insertSubview:alternative atIndex:1];
 update_switcher_wallpaper(root);assert(replica.image==d&&windowScans==3);
 puts("PASS actual Search/switcher: native capsule clipping follows bounds, sibling material scoped, visibility retained/no new plates; 600 layouts/one window scan, image changes, resize, detach and empty-source recovery");
}}
'''
with tempfile.TemporaryDirectory(prefix='river-search-switcher-') as d:
 p=Path(d);(p/'test.m').write_text(pre+clip+wall+main)
 subprocess.run(['clang','-fobjc-arc','-framework','Foundation','-framework','CoreGraphics',str(p/'test.m'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
