from pathlib import Path
import subprocess,tempfile
from river_audio_test_source import source as actual_source
s=actual_source('river_codec_probe.h');s=s[s.index('static void\nri_codec_spi_delay'):s.index('struct ri_codec_saved_pins')]
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#define NSEC_PER_USEC 1000
static unsigned ri_codec_pin[4]={0,1,2,3};
static uint64_t now,mosi_at,high_at,low_at,selected_at;
static unsigned edges,reads,byte_index,rx_bit;
static uint8_t tx_seen[8],rx_bytes[8];
static bool high,selected,jitter;
static void clock_interval_to_absolutetime_interval(unsigned n,unsigned scale,uint64_t *out){assert(n==5&&scale==1000);*out=n*scale;}
static uint64_t mach_absolute_time(void){now+=100;if(jitter&&now%1700==0)now+=30000;return now;}
static void ri_wr(unsigned pin,unsigned off,unsigned v){assert(off==4 && (v==0||v==2));
 if(pin==1){if(!v){assert(!selected&&!high);selected=true;selected_at=now;byte_index=rx_bit=0;}else{assert(selected&&!high&&now>=low_at);selected=false;}return;}
 assert(selected);
 if(pin==2){assert(!high);mosi_at=now;tx_seen[byte_index]=(uint8_t)((tx_seen[byte_index]<<1)|(v?1:0));}
 else{assert(pin==0);if(v){assert(!high&&now-mosi_at>=5000&&now-selected_at>=5000);high=true;high_at=now;edges++;}else{assert(high&&now-high_at>=5000);high=false;low_at=now;}}
}
static unsigned ri_rd(unsigned pin,unsigned off){assert(pin==3&&off==4&&selected&&high&&now-high_at>=5000);reads++;unsigned out=(rx_bytes[byte_index]>>(7-rx_bit))&1;if(++rx_bit==8){rx_bit=0;byte_index++;}return out;}
'''
main=r'''
int main(void){for(unsigned j=0;j<2;j++){now=edges=reads=byte_index=rx_bit=0;selected=high=false;jitter=j;for(unsigned i=0;i<8;i++){tx_seen[i]=0;rx_bytes[i]=0;}rx_bytes[6]=0x63;rx_bytes[7]=0x60;
 assert(ri_codec_id_read(0)==0x6360);assert(!selected&&!high&&edges==64&&reads==64&&tx_seen[0]==0x80);for(unsigned i=1;i<8;i++)assert(tx_seen[i]==0);
 unsigned before=edges;assert(ri_codec_id_read(2)==0xffff&&edges==before);assert(now>=650000);
 }puts("PASS actual codec SPI: 64 ordered mode-0 edges, five-us MOSI/high/CS timing, exact 0x6360 read, invalid register refusal and stretched/preempted edges; no scheduler sleeps");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text(pre+s+main);subprocess.run(['cc','-std=c11','-O1','-fsanitize=address,undefined',str(d/'test.c'),'-o',str(d/'test')],check=True);subprocess.run([str(d/'test')],check=True)
