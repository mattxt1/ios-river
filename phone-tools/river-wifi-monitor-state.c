/* Read-only bounded monitor diagnostics. Output is private validation data.
 * No DHCP request, SC publication, route, resolver or credentials operation. */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <CommonCrypto/CommonDigest.h>
static int tail_file(const char *path,size_t limit) {
 printf("file_open_begin=%s\n",path);
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0){printf("file_open_error=%d\n",errno);return 1;}
 struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0){puts("file_refused=nonregular_or_stat_error");close(fd);return 1;}
 off_t start=st.st_size>(off_t)limit?st.st_size-(off_t)limit:0;
 printf("file_size=%lld offset=%lld limit=%zu\n",(long long)st.st_size,(long long)start,limit);
 if(lseek(fd,start,SEEK_SET)!=start){perror("file seek");close(fd);return 1;}
 char data[4096];size_t total=0;
 while(total<limit){size_t want=limit-total;if(want>sizeof(data))want=sizeof(data);
  ssize_t n=read(fd,data,want);if(n<0&&errno==EINTR)continue;if(n<0){perror("file read");close(fd);return 1;}if(!n)break;
  if(fwrite(data,1,(size_t)n,stdout)!=(size_t)n){close(fd);return 1;}total+=(size_t)n;
 }
 close(fd);printf("\nfile_read_complete bytes=%zu\n",total);return 0;
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static int helper_hash(void) {
 const char *path="/usr/local/bin/river-net";printf("hash_open_begin=%s\n",path);
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0){perror("helper open");return 1;}
 struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||st.st_size>4*1024*1024){puts("helper_refused=unexpected_size_or_type");close(fd);return 1;}
 CC_SHA256_CTX ctx;CC_SHA256_Init(&ctx);unsigned char data[4096],digest[32];size_t total=0;
 while(total<(size_t)st.st_size){size_t want=(size_t)st.st_size-total;if(want>sizeof(data))want=sizeof(data);ssize_t n=read(fd,data,want);
  if(n<0&&errno==EINTR)continue;if(n<=0){puts("helper_read_incomplete");close(fd);return 1;}CC_SHA256_Update(&ctx,data,(CC_LONG)n);total+=(size_t)n;
 }
 close(fd);CC_SHA256_Final(digest,&ctx);printf("helper_bytes=%zu sha256=",total);for(unsigned i=0;i<sizeof(digest);i++)printf("%02x",digest[i]);putchar('\n');return 0;
}
#pragma clang diagnostic pop
int main(void) {
 setvbuf(stdout,NULL,_IONBF,0);
 int error=helper_hash();
 error|=tail_file("/Library/LaunchDaemons/org.river.net.plist",8192);
 error|=tail_file("/tmp/river-net.log",65536);
 return error;
}
