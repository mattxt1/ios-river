#!/usr/bin/env python3
"""Exercise the actual folder hook with a small Objective-C geometry model."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parent.parent
s=(repo/'phone-tools/cgprobe.c').read_text();a=s.index('static void probe_folder_bg_layout(');b=s.index('{',a);i=b+1;depth=1
while depth:
 if s[i]=='{':depth+=1
 elif s[i]=='}':depth-=1
 i+=1
actual=s[a:i]
pre=r'''
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <assert.h>
#include <string.h>
@interface TraceLayer:NSObject
@property double cornerRadius;
@property BOOL masksToBounds;
@property id filters;
@property(nonatomic) CGColorRef backgroundColor;
@end
@implementation TraceLayer
- (void)setBackgroundColor:(CGColorRef)c {if(c)CFRetain(c);if(_backgroundColor)CFRelease(_backgroundColor);_backgroundColor=c;}
- (void)dealloc {if(_backgroundColor)CFRelease(_backgroundColor);}
@end
@interface TraceView:NSObject
@property BOOL hidden;
@property double alpha;
@property CGRect bounds;
@property TraceLayer *layer;
@end
@implementation TraceView
- (instancetype)init {if((self=[super init])){_layer=[TraceLayer new];_alpha=1;}return self;}
@end
@interface TestFolderBackgroundView:TraceView {
@public TraceView *_tintView,*_blurView;
}
@end
@implementation TestFolderBackgroundView
- (instancetype)init {if((self=[super init])){_tintView=[TraceView new];_blurView=[TraceView new];}return self;}
@end
static unsigned native_calls;
static void native_layout(id view,SEL cmd) {native_calls++;((TraceView*)view).alpha=.25;((TraceView*)view).hidden=YES;}
static void (*original_folder_bg_layout)(id,SEL)=native_layout;
'''
main=r'''
int main(void){@autoreleasepool{
 SEL layout=sel_registerName("layoutSubviews");
 TestFolderBackgroundView *v=[TestFolderBackgroundView new];v.bounds=CGRectMake(0,0,300,400);v.layer.cornerRadius=19;v.layer.filters=@[@"filter"];
 probe_folder_bg_layout(v,layout);
 assert(native_calls==1&&v.hidden&&v.alpha==.25&&v.layer.cornerRadius==19&&v.layer.masksToBounds&&!v.layer.filters);
 assert(CGColorGetAlpha(v.layer.backgroundColor)>.379&&CGColorGetAlpha(v.layer.backgroundColor)<.381&&v->_blurView.hidden&&v->_tintView.hidden);
 TestFolderBackgroundView *small=[TestFolderBackgroundView new];small.bounds=CGRectMake(0,0,60,60);probe_folder_bg_layout(small,layout);
 assert(small.layer.cornerRadius==14&&CGColorGetAlpha(small.layer.backgroundColor)>.319&&CGColorGetAlpha(small.layer.backgroundColor)<.321);
 TestFolderBackgroundView *empty=[TestFolderBackgroundView new];empty.bounds=CGRectMake(0,0,0,0);probe_folder_bg_layout(empty,layout);assert(!empty.layer.backgroundColor&&!empty->_blurView.hidden);
 TraceView *other=[TraceView new];other.bounds=CGRectMake(0,0,300,400);probe_folder_bg_layout(other,layout);assert(!other.layer.backgroundColor);
 assert(native_calls==4);puts("PASS actual folder fallback: one painted layer, native opacity/visibility/corner radius preserved, exact fallback radii/alpha, zero-size/non-folder refusal");return 0;
}}
'''
with tempfile.TemporaryDirectory(prefix='river-folder-test-') as d:
 d=Path(d);(d/'test.m').write_text(pre+actual+main)
 subprocess.run(['clang','-fobjc-arc','-framework','Foundation','-framework','CoreGraphics',str(d/'test.m'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
