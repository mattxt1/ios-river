#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
extern mach_port_t bootstrap_port;
extern kern_return_t bootstrap_look_up(mach_port_t,const char *,mach_port_t *);
static void *deadline(void *p){sleep(5);fprintf(stderr,"discovery_deadline_exceeded=1\n");fflush(NULL);_exit(124);return NULL;}
int main(void){@autoreleasepool{
 pthread_t thread;if(pthread_create(&thread,NULL,deadline,NULL))return 2;
 mach_port_t service=MACH_PORT_NULL;kern_return_t kr=bootstrap_look_up(bootstrap_port,"com.apple.coremedia.capturesource",&service);
 printf("capturesource_bootstrap_result=%d advertised=%d\n",kr,kr==KERN_SUCCESS);fflush(stdout);
 if(kr==KERN_SUCCESS)mach_port_deallocate(mach_task_self(),service);
 puts("video_discovery_begin=1 recording_requested=0");fflush(stdout);
 mach_timebase_info_data_t tb;mach_timebase_info(&tb);uint64_t t=mach_absolute_time();
 NSArray *devices=[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
 printf("video_discovery_returned=1 devices=%lu elapsed_ms=%llu recording_requested=0\n",(unsigned long)devices.count,(mach_absolute_time()-t)*tb.numer/tb.denom/1000000ull);fflush(stdout);return 0;
}}
