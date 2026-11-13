/* Bounded connectivity probe bound to en0, never the USB interface. Uses
 * Apple's public captive-portal HTTP test; no credentials or response body
 * are printed. A response is a connectivity observation, not a TLS test. */
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static void expired(int signo)
{
 (void)signo;
 const char message[]="Wi-Fi probe exceeded 20 seconds\n";
 (void)write(STDERR_FILENO,message,sizeof(message)-1);
 _exit(124);
}

int main(int argc, char **argv)
{
 if (argc > 2) { fputs("usage: river-wifi-check [resolved-Apple-IPv4]\n",stderr); return 2; }
 struct in_addr numeric;
 if (argc == 2 && inet_pton(AF_INET,argv[1],&numeric) != 1) {
  fputs("Expected numeric IPv4 address\n",stderr); return 2;
 }
 signal(SIGALRM, expired);
 alarm(20); /* Bounds DNS, connect and all I/O together. */
 signal(SIGPIPE, SIG_IGN);
 unsigned index = if_nametoindex("en0");
 if (!index) { perror("en0"); return 1; }
 struct addrinfo hints = { .ai_family=AF_INET, .ai_socktype=SOCK_STREAM }, *list=NULL;
 if (argc == 2) hints.ai_flags = AI_NUMERICHOST;
 int error = getaddrinfo(argc == 2 ? argv[1] : "captive.apple.com", "80", &hints, &list);
 if (error) { fprintf(stderr,"DNS: %s\n",gai_strerror(error)); return 1; }
 int fd = -1;
 for (struct addrinfo *it=list; it; it=it->ai_next) {
  fd=socket(it->ai_family,it->ai_socktype,it->ai_protocol);
  if (fd<0) continue;
  if (setsockopt(fd,IPPROTO_IP,IP_BOUND_IF,&index,sizeof(index))) {
   perror("IP_BOUND_IF"); close(fd); freeaddrinfo(list); return 1;
  }
  if (!connect(fd,it->ai_addr,it->ai_addrlen)) break;
  close(fd); fd=-1;
 }
 freeaddrinfo(list);
 if (fd<0) { perror("connect on en0"); return 1; }
 unsigned bound=0; socklen_t bound_size=sizeof(bound);
 if (getsockopt(fd,IPPROTO_IP,IP_BOUND_IF,&bound,&bound_size) || bound!=index) {
  fputs("Socket interface binding changed\n",stderr); close(fd); return 1;
 }
 const char request[]="GET /hotspot-detect.html HTTP/1.1\r\nHost: captive.apple.com\r\nConnection: close\r\n\r\n";
 size_t sent=0;
 while (sent<sizeof(request)-1) {
  ssize_t n=write(fd,request+sent,sizeof(request)-1-sent);
  if (n<0 && errno==EINTR) continue;
  if (n<=0) { perror("send"); close(fd); return 1; }
  sent+=(size_t)n;
 }
 char response[4096]; size_t count=0;
 while (count<sizeof(response)-1) {
  ssize_t n=read(fd,response+count,sizeof(response)-1-count);
  if (n<0 && errno==EINTR) continue;
  if (n<0) { perror("receive"); close(fd); return 1; }
  if (!n) break;
  count+=(size_t)n;
 }
 close(fd); response[count]=0; alarm(0);
 int status=0;
 if (sscanf(response,"HTTP/%*u.%*u %d",&status)!=1) {
  fputs("Invalid HTTP response\n",stderr); return 1;
 }
 char *body=strstr(response,"\r\n\r\n");
 int success=body && strstr(body+4,"<TITLE>Success</TITLE>")!=NULL;
 printf("interface=en0 bound_index=%u numeric_target=%d http_status=%d bytes=%zu apple_success_marker=%d\n",
     bound,argc==2,status,count,success);
 return status==200 && success ? 0 : 1;
}
