#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <CommonCrypto/CommonDigest.h>
static int metadata_test(const char *path) {
 unsigned char data[7516];struct stat st;
 int fd=open(path,O_RDONLY|O_NOFOLLOW);
 if(fd<0) {perror(path);return 1;}
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=(off_t)sizeof(data)) {
  fprintf(stderr,"Expected regular 7516-byte stock ADSP metadata file\n");close(fd);return 1;
 }
 size_t n=0;
 while(n<sizeof(data)) {
  ssize_t count=read(fd,data+n,sizeof(data)-n);
  if(count<0&&errno==EINTR) continue;
  if(count<=0) {fprintf(stderr,"Incomplete metadata read\n");close(fd);return 1;}
  n+=(size_t)count;
 }
 close(fd);
 if(sysctlbyname("debug.river_adsp_metadata",NULL,NULL,data,sizeof(data))) {
  perror("ADSP metadata authentication");return 1;
 }
 return 0;
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static int verify_image(int fd) {
 static const unsigned char expected[32]={
  0xf3,0x25,0x94,0x2f,0x83,0x05,0x73,0x64,0xdc,0xab,0xe7,0x59,0xd3,0x50,0xb4,0x04,
  0x39,0xfa,0x08,0xc2,0xe4,0xed,0xe9,0x08,0x53,0x05,0x0e,0x15,0xa5,0x2e,0x01,0xa5};
 CC_SHA256_CTX ctx;unsigned char buf[0x4000],digest[32];size_t total=0;
 CC_SHA256_Init(&ctx);
 for(;;) {
  ssize_t n=read(fd,buf,sizeof(buf));
  if(n<0&&errno==EINTR) continue;
  if(n<0) return 1;
  if(!n) break;
  total+=(size_t)n;if(total>0x1100000) return 1;
  CC_SHA256_Update(&ctx,buf,(CC_LONG)n);
 }
 CC_SHA256_Final(digest,&ctx);
 return total!=0x1100000 || memcmp(digest,expected,sizeof(digest)) || lseek(fd,0,SEEK_SET)!=0;
}
#pragma clang diagnostic pop
static int stage_image(const char *path) {
 struct {unsigned int operation,offset,length;unsigned char data[0x4000];} packet;
 struct stat st;int fd=open(path,O_RDONLY|O_NOFOLLOW);
 if(fd<0) {perror(path);return 1;}
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=0x1100000) {
  fprintf(stderr,"Expected regular 17MiB validated ADSP image\n");close(fd);return 1;
 }
 if(verify_image(fd)) {
  fprintf(stderr,"Stock ADSP image hash verification failed before memory setup\n");close(fd);return 1;
 }
 packet.operation=0x41445301;packet.offset=packet.length=0;
 if(sysctlbyname("debug.river_adsp_stage",NULL,NULL,&packet,12)) {
  perror("ADSP memory setup");close(fd);return 1;
 }
 for(unsigned offset=0;offset<0x1100000;offset+=0x4000) {
  size_t got=0;
  while(got<sizeof(packet.data)) {
   ssize_t n=read(fd,packet.data+got,sizeof(packet.data)-got);
   if(n<0&&errno==EINTR) continue;
   if(n<=0) {fprintf(stderr,"Incomplete image; staging left incomplete\n");close(fd);return 1;}
   got+=(size_t)n;
  }
  packet.operation=0x41445302;packet.offset=offset;packet.length=sizeof(packet.data);
  if(sysctlbyname("debug.river_adsp_stage",NULL,NULL,&packet,sizeof(packet))) {
   fprintf(stderr,"ADSP staging failed at offset %#x: %s\n",offset,strerror(errno));close(fd);return 1;
  }
  if(!((offset+0x4000)&0xfffff)) {printf("Copied %u MiB; DSP start not requested\n",(offset+0x4000)>>20);fflush(stdout);}
 }
 close(fd);packet.operation=0x41445303;packet.offset=packet.length=0;
 if(sysctlbyname("debug.river_adsp_stage",NULL,NULL,&packet,12)) {perror("ADSP image digest");return 1;}
 return 0;
}
static int file_sha256(const char *path) {
 int fd=open(path,O_RDONLY|O_NOFOLLOW);struct stat st;
 if(fd<0){perror("Hash input");return 1;}
 if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || st.st_size>64*1024*1024) {
  fprintf(stderr,"Hash input must be a regular file of at most64MiB\n");close(fd);return 1;
 }
 CC_SHA256_CTX state;unsigned char buffer[32768],digest[CC_SHA256_DIGEST_LENGTH];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
 CC_SHA256_Init(&state);off_t total=0;
 while(total<st.st_size) {
  size_t count=(size_t)(st.st_size-total);if(count>sizeof(buffer))count=sizeof(buffer);
  ssize_t n=read(fd,buffer,count);if(n<0 && errno==EINTR)continue;
  if(n<=0){perror("Hash read");close(fd);return 1;}
  CC_SHA256_Update(&state,buffer,(CC_LONG)n);total+=n;
 }
 CC_SHA256_Final(digest,&state);
#pragma clang diagnostic pop
 close(fd);printf("sha256=");for(unsigned i=0;i<sizeof(digest);i++)printf("%02x",digest[i]);
 printf(" bytes=%lld\n",(long long)total);return 0;
}
int main(int argc,char **argv) {
 if(argc==3 && !strcmp(argv[1],"--file-sha256"))return file_sha256(argv[2]);
 int rc=0,request=1;
 const char *status="debug.river_adsp_status";
 if(argc==2 && (!strcmp(argv[1],"--mic-capture") || !strcmp(argv[1],"--mic-status"))) {
  status="debug.river_mic_capture_status";
  if(!strcmp(argv[1],"--mic-capture") && sysctlbyname("debug.river_mic_capture",NULL,NULL,&request,sizeof(request))) {perror("Physical microphone capture");rc=1;}
 } else
 if(argc==2 && (!strcmp(argv[1],"--auto-enable") || !strcmp(argv[1],"--auto-disable") || !strcmp(argv[1],"--auto-status"))) {
  status="debug.river_audio_auto_status";request=strcmp(argv[1],"--auto-disable")?1:0;
  if(strcmp(argv[1],"--auto-status") && sysctlbyname("debug.river_audio_auto",NULL,NULL,&request,sizeof(request))) {perror("Native audio lifecycle");rc=1;}
 } else
 if(argc==2 && (!strcmp(argv[1],"--queue-arm") || !strcmp(argv[1],"--queue-run") || !strcmp(argv[1],"--queue-stop") || !strcmp(argv[1],"--queue-status") || !strcmp(argv[1],"--queue-arm-long") || !strcmp(argv[1],"--queue-run-long") || !strcmp(argv[1],"--queue-go"))) {
  status="debug.river_audio_queue_status";
  if(!strcmp(argv[1],"--queue-arm-long"))request=5;
  if(!strcmp(argv[1],"--queue-go"))request=6;
  if(!strcmp(argv[1],"--queue-run-long"))request=4;
  if(!strcmp(argv[1],"--queue-run"))request=2;
  if(!strcmp(argv[1],"--queue-stop"))request=3;
  if(strcmp(argv[1],"--queue-status") && sysctlbyname("debug.river_audio_queue",NULL,NULL,&request,sizeof(request))) {perror("Live PCM queue");rc=1;}
 } else
 if(argc==2 && (!strcmp(argv[1],"--native-stage-arm") || !strcmp(argv[1],"--native-stage-status"))) {
  status="debug.river_native_pcm_status";
  if(!strcmp(argv[1],"--native-stage-arm") && sysctlbyname("debug.river_native_pcm_stage",NULL,NULL,&request,sizeof(request))) {perror("Native PCM staging");rc=1;}
 } else
 if(argc==2 && (!strcmp(argv[1],"--codec-route") || !strcmp(argv[1],"--codec-route-status") || !strcmp(argv[1],"--codec-pcm") || !strcmp(argv[1],"--codec-tone") || !strcmp(argv[1],"--codec-native-pcm"))) {
  status="debug.river_adsp_codec_path_status";
  if(!strcmp(argv[1],"--codec-pcm"))request=2;
  if(!strcmp(argv[1],"--codec-tone"))request=3;
  if(!strcmp(argv[1],"--codec-native-pcm"))request=4;
  if((!strcmp(argv[1],"--codec-route") || !strcmp(argv[1],"--codec-pcm") || !strcmp(argv[1],"--codec-tone") || !strcmp(argv[1],"--codec-native-pcm")) && sysctlbyname("debug.river_adsp_codec_path",NULL,NULL,&request,sizeof(request))) {perror("Muted codec route test");rc=1;}
 } else if(argc==2 && (!strcmp(argv[1],"--channel-test") || !strcmp(argv[1],"--channel-status"))) {
  status="debug.river_adsp_channel_status";
  if(!strcmp(argv[1],"--channel-test") && sysctlbyname("debug.river_adsp_channel",NULL,NULL,&request,sizeof(request))) {perror("SLIMbus channel test");rc=1;}
 } else if(argc==2 && (!strcmp(argv[1],"--ngd-enumerate") || !strcmp(argv[1],"--ngd-status"))) {
  status="debug.river_adsp_ngd_status";
  if(!strcmp(argv[1],"--ngd-enumerate") && sysctlbyname("debug.river_adsp_ngd",NULL,NULL,&request,sizeof(request))) {perror("NGD capability/address test");rc=1;}
 } else if(argc==2 && (!strcmp(argv[1],"--slim-discover") || !strcmp(argv[1],"--slim-power") || !strcmp(argv[1],"--slim-ngd") || !strcmp(argv[1],"--slim-status"))) {
  status="debug.river_adsp_slim_status";
  if(strcmp(argv[1],"--slim-status")) {
   request=!strcmp(argv[1],"--slim-discover")?1:!strcmp(argv[1],"--slim-power")?2:3;
   if(sysctlbyname("debug.river_adsp_slim",NULL,NULL,&request,sizeof(request))) {perror("SLIMbus prerequisite");rc=1;}
  }
 } else if(argc==2 && (!strcmp(argv[1],"--pcm-silence") || !strcmp(argv[1],"--stream-status"))) {
  status="debug.river_adsp_stream_status";
  if(!strcmp(argv[1],"--pcm-silence") && sysctlbyname("debug.river_adsp_stream",NULL,NULL,&request,sizeof(request))) {
   perror("Zero-PCM completion test");rc=1;
  }
 } else if(argc==2 && (!strcmp(argv[1],"--afe-config") || !strcmp(argv[1],"--adm-route") ||
     !strcmp(argv[1],"--afe-route") || !strcmp(argv[1],"--route-status"))) {
  status="debug.river_adsp_route_status";
  if(strcmp(argv[1],"--route-status")) {
   request=!strcmp(argv[1],"--afe-config")?1:!strcmp(argv[1],"--adm-route")?2:3;
   if(sysctlbyname("debug.river_adsp_route",NULL,NULL,&request,sizeof(request))) {
    perror("AFE/ADM route test");rc=1;
   }
  }
 } else if(argc==2 && (!strcmp(argv[1],"--pcm-map") || !strcmp(argv[1],"--asm-test") ||
     !strcmp(argv[1],"--afe-probe") || !strcmp(argv[1],"--pcm-status"))) {
  status="debug.river_adsp_pcm_status";
  if(strcmp(argv[1],"--pcm-status")) {
   request=!strcmp(argv[1],"--pcm-map")?1:!strcmp(argv[1],"--asm-test")?2:3;
   if(sysctlbyname("debug.river_adsp_pcm",NULL,NULL,&request,sizeof(request))) {
    perror("PCM/ASM/AFE prerequisite test");rc=1;
   }
  }
 } else if(argc==2 && (!strcmp(argv[1],"--apr") || !strcmp(argv[1],"--apr-status"))) {
  status="debug.river_adsp_apr_status";
  if(!strcmp(argv[1],"--apr") && sysctlbyname("debug.river_adsp_apr",NULL,NULL,&request,sizeof(request))) {
   perror("APR CORE state query");rc=1;
  }
 } else if(argc==2 && (!strcmp(argv[1],"--start") || !strcmp(argv[1],"--start-status"))) {
  status="debug.river_adsp_start_status";
  if(!strcmp(argv[1],"--start") && sysctlbyname("debug.river_adsp_start",NULL,NULL,&request,sizeof(request))) {
   perror("ADSP startup");rc=1;
  }
 } else if(argc==2&&!strcmp(argv[1],"--probe")) {
  if(sysctlbyname("debug.river_adsp_probe",NULL,NULL,&request,sizeof(request))) {
   perror("ADSP prerequisite probe");rc=1;
  }
 } else if(argc==2&&!strcmp(argv[1],"--handshake-init")) {
  if(sysctlbyname("debug.river_adsp_handshake",NULL,NULL,&request,sizeof(request))) {
   perror("ADSP handshake initialization");rc=1;
  }
 } else if(argc==3&&!strcmp(argv[1],"--metadata")) rc=metadata_test(argv[2]);
 else if(argc==3&&!strcmp(argv[1],"--stage-image")) rc=stage_image(argv[2]);
 else if(argc!=1) {
  fprintf(stderr,"Usage: %s [--probe|--handshake-init|--metadata PATH|--stage-image PATH|--start|--start-status|--apr|--apr-status|--pcm-map|--asm-test|--afe-probe|--pcm-status|--afe-config|--adm-route|--afe-route|--route-status|--pcm-silence|--stream-status|--slim-discover|--slim-power|--slim-ngd|--slim-status|--ngd-enumerate|--ngd-status|--channel-test|--channel-status|--codec-route|--codec-route-status|--codec-pcm|--codec-tone|--codec-native-pcm|--native-stage-arm|--native-stage-status|--queue-arm|--queue-run|--queue-stop|--queue-status|--queue-arm-long|--queue-run-long|--queue-go|--file-sha256 PATH|--auto-enable|--auto-disable|--auto-status|--mic-capture|--mic-status]\n",argv[0]);return 2;
 }
 char note[8192]={0};size_t n=sizeof(note)-1;
 if(sysctlbyname(status,note,&n,NULL,0)) {perror("ADSP status");return 1;}
 puts(note);return rc;
}
