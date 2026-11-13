#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
 int rc=0,request=1;
 if(argc==2&&!strcmp(argv[1],"--probe")) {
  if(sysctlbyname("debug.river_adsp_probe",NULL,NULL,&request,sizeof(request))) {
   perror("ADSP prerequisite probe");rc=1;
  }
 } else if(argc!=1) {
  fprintf(stderr,"Usage: %s [--probe]\n",argv[0]);return 2;
 }
 char note[8192]={0};size_t n=sizeof(note)-1;
 if(sysctlbyname("debug.river_adsp_status",note,&n,NULL,0)) {perror("ADSP status");return 1;}
 puts(note);return rc;
}
