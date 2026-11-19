#!/usr/bin/env python3
"""Exercise actual metadata-ring source extracted from River's kernel patch."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parent.parent
patch=next((repo/'xnu/patches').glob('0342-*')).read_text()
section=next(x for x in patch.split('diff --git ')[1:] if x.startswith('a/iokit/Kernel/arm/river_fb_composition_trace.h '))
header='\n'.join(x[1:] for x in section.splitlines() if x.startswith('+') and not x.startswith('+++'))+'\n'
body=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "trace.h"
int main(void) {
 struct river_fb_trace_ring ring = {0};
 struct river_fb_trace_packet packet = {0};
 _Static_assert(sizeof(packet) < 4096,"IOKit inline output limit");
 river_fb_trace_read(&ring,0,&packet);assert(packet.available==0 && packet.count==0 && packet.version==1);
 for(unsigned i=0;i<600;i++) {
  struct river_fb_trace_record r={0};r.time_ns=i*16666667ULL;r.swap_id=i;r.mask=(i%15)+1;
  for(unsigned l=0;l<4;l++){r.surface[l]=i*4+l;for(unsigned j=0;j<4;j++) {
   r.source_rect[l][j]=0xffff0000u+i*16+l*4+j;r.destination_rect[l][j]=i*16+l*4+j;
  }}river_fb_trace_append(&ring,&r);
 }
 unsigned seen=0;
 for(unsigned first=0;first<=256;first+=16) {
  river_fb_trace_read(&ring,first,&packet);assert(packet.available==256 && packet.total==600 && packet.first==first);
  assert(packet.count==(first<256?16:0));
  for(unsigned k=0;k<packet.count;k++) {
   unsigned i=344+first+k;struct river_fb_trace_record *r=&packet.records[k];
   assert(r->ordinal==i && r->swap_id==i && r->time_ns==i*16666667ULL && r->mask==i%15+1);
   for(unsigned l=0;l<4;l++){assert(r->surface[l]==i*4+l);for(unsigned j=0;j<4;j++){
    assert(r->source_rect[l][j]==0xffff0000u+i*16+l*4+j);assert(r->destination_rect[l][j]==i*16+l*4+j);
   }}seen++;
  }
 }
 assert(seen==256);river_fb_trace_read(&ring,0xffffffffu,&packet);assert(!packet.count);
 puts("PASS actual composition trace: empty/wrapped chronological ring, exact signed-rect bits, bounded inline chunks and out-of-range reads");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'trace.h').write_text(header);(p/'test.c').write_text(body)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
