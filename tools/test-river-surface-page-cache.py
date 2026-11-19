#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,re
repo=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory() as d:
 root=Path(d);path='iokit/Kernel/arm/SweetSurface.cpp';(root/path).parent.mkdir(parents=True)
 for patch in sorted((repo/'xnu/patches').glob('*.patch')):
  text=patch.read_bytes().decode('utf-8',errors='replace')
  sections=re.findall(r'(?ms)^--- [^\n]+\n\+\+\+ b/'+re.escape(path)+r'\n.*?(?=^diff --git |^--- |\Z)',text)
  selected=''.join(sections)
  if selected:subprocess.run(['patch','--batch','--silent','--fuzz=0','-p1','-d',d],input=selected.encode(),check=True)
 s=(root/path).read_text()
a=s.index('bool\nSweetSurface::getBlitPages(');b=s.index('\n#endif',a);actual=s[a:b].replace('SweetSurface::','MockSurface::')
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>
using IOByteCount=uint64_t;
#define PAGE_SIZE 16384ULL
#define PAGE_MASK (PAGE_SIZE-1)
static unsigned allocations,frees;static bool failAlloc;
static void *IOMallocData(size_t n){if(failAlloc)return nullptr;allocations++;return malloc(n);}
static void IOFreeData(void *p,size_t n){assert(p&&n);frees++;free(p);}
static const int kIOMemoryMapperNone=0;
struct IOMemoryDescriptor {
 uint64_t length=PAGE_SIZE*3-11,low=0;unsigned walks=0,failAt=9999;
 std::vector<uint64_t> physical={0x10000000,0x20000000,0x30000000,0x40000000};
 uint64_t getLength(){return length;}
 uint64_t getPhysicalSegment(uint64_t at,IOByteCount *seg,int mapper){assert(!mapper);walks++;if(at>=length)return 0;uint64_t pos=low+at;unsigned i=pos/PAGE_SIZE;if(i==failAt)return 0;*seg=PAGE_SIZE-(pos&PAGE_MASK);return physical[i]+(pos&PAGE_MASK);}
};
struct MockSurface {
 IOMemoryDescriptor *memory=nullptr;uint32_t offset=0;
 uint64_t *riverBlitPageList=nullptr;IOMemoryDescriptor *riverBlitPageOwner=nullptr;
 uint64_t riverBlitPageLength=0,riverBlitFirstOffset=0;uint32_t riverBlitPageCount=0;
 bool getBlitPages(const uint64_t **,uint32_t *,uint64_t *,bool *);
 ~MockSurface(){if(riverBlitPageList)IOFreeData(riverBlitPageList,riverBlitPageCount*sizeof(uint64_t));}
};
'''
main=r'''
int main(){const uint64_t *p;uint32_t n;uint64_t start;bool hit;
 {IOMemoryDescriptor mem;MockSurface s;s.memory=&mem;
 assert(s.getBlitPages(&p,&n,&start,&hit)&&!hit&&n==3&&start==0&&mem.walks==3);
 auto first=p;for(int i=0;i<600;i++)assert(s.getBlitPages(&p,&n,&start,&hit)&&hit&&p==first);
 assert(mem.walks==3&&allocations==1&&!frees);
 s.offset=PAGE_SIZE;assert(s.getBlitPages(&p,&n,&start,&hit)&&start==PAGE_SIZE&&n==3&&hit);
 mem.length--;assert(!s.getBlitPages(&p,&n,&start,&hit)&&!p&&!n);
 }
 assert(frees==1);
 {IOMemoryDescriptor mem;mem.low=113;mem.length=PAGE_SIZE*2;MockSurface s;s.memory=&mem;s.offset=97;
 assert(s.getBlitPages(&p,&n,&start,&hit)&&n==3&&start==210);
 for(unsigned i=0;i<n;i++)assert(p[i]==mem.physical[i]);}
 {IOMemoryDescriptor mem;mem.failAt=1;MockSurface s;s.memory=&mem;
 assert(!s.getBlitPages(&p,&n,&start,&hit)&&!s.riverBlitPageList);mem.failAt=9999;
 assert(s.getBlitPages(&p,&n,&start,&hit)&&!hit);}
 {IOMemoryDescriptor mem;MockSurface s;s.memory=&mem;failAlloc=true;
 assert(!s.getBlitPages(&p,&n,&start,&hit)&&!s.riverBlitPageList);failAlloc=false;
 assert(s.getBlitPages(&p,&n,&start,&hit));IOMemoryDescriptor other;s.memory=&other;
 assert(!s.getBlitPages(&p,&n,&start,&hit));}
 {MockSurface s;assert(!s.getBlitPages(&p,&n,&start,&hit));IOMemoryDescriptor mem;s.memory=&mem;
 mem.length=0;assert(!s.getBlitPages(&p,&n,&start,&hit));mem.length=10;s.offset=10;assert(!s.getBlitPages(&p,&n,&start,&hit));
 mem.length=~0ULL;s.offset=0;assert(!s.getBlitPages(&p,&n,&start,&hit));}
 assert(allocations==frees);puts("PASS actual page cache:600 reuse hits/one allocation, immutable backing, unaligned descriptors/plane offsets, partial map/allocation failure retry, overflow/refusal and balanced frees");}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.cpp').write_text(pre+actual+main);subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(d/'test.cpp'),'-o',str(d/'test')],check=True);subprocess.run([str(d/'test')],check=True)
