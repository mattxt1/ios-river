#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
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
int main(int argc,char **argv) {
 int rc=0,request=1;
 if(argc==2&&!strcmp(argv[1],"--probe")) {
  if(sysctlbyname("debug.river_adsp_probe",NULL,NULL,&request,sizeof(request))) {
   perror("ADSP prerequisite probe");rc=1;
  }
 } else if(argc==3&&!strcmp(argv[1],"--metadata")) rc=metadata_test(argv[2]);
 else if(argc!=1) {
  fprintf(stderr,"Usage: %s [--probe|--metadata PATH]\n",argv[0]);return 2;
 }
 char note[8192]={0};size_t n=sizeof(note)-1;
 if(sysctlbyname("debug.river_adsp_status",note,&n,NULL,0)) {perror("ADSP status");return 1;}
 puts(note);return rc;
}
