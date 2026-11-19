from pathlib import Path
import re, subprocess,tempfile,sys
root=Path(sys.argv[1]).resolve()
sweet=root/'src/gallium/drivers/freedreno/sweetgl'
s=(sweet/'eagl.c').read_text();clear=s[s.index('void\nsgl_Clear('):s.index('\nvoid\nsgl_BindFramebufferEXT(')]
# Execute the actual clear body with state reads counted, for every trace gate.
pre='''#include <assert.h>
#include <stdint.h>
typedef int GLint; typedef float GLfloat; typedef unsigned GLbitfield;
#define GL_FRAMEBUFFER_BINDING 1
#define GL_COLOR_CLEAR_VALUE 2
#define GL_SCISSOR_BOX 3
#define GL_SCISSOR_TEST 4
static int trace_left,win_left,queries,clears,posts,binds; static unsigned mask_seen;
#define SGL_CUR(x) (++binds)
#define SGL_POST(x) (++posts)
#define WTRACE(...) ((void)0)
static void glGetIntegerv(int x,int *p){queries++;}
static void glGetFloatv(int x,float *p){queries++;}
static int glIsEnabled(int x){queries++;return 0;}
static void glClear(unsigned m){clears++;mask_seen=m;}
'''
main='''int main(void){for(int a=0;a<2;a++)for(int b=0;b<2;b++){trace_left=a;win_left=b;queries=0;sgl_Clear(0,0x4100);assert(queries==((a&&b)?3:0));assert(mask_seen==0x4100);}assert(clears==4&&posts==4&&binds==4);return 0;}'''
# Run generator against tiny GL signatures, testing both a direct-return slot and special void slot.
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'GL').mkdir();(d/'GL/gl.h').write_text('GLAPI GLint GLAPIENTRY glGetInteger (GLenum pname);\nGLAPI void GLAPIENTRY glClear (GLbitfield mask);\n');(d/'GL/glext.h').write_text('');(d/'layout').write_text('1 glGetInteger\n10 glClear\n');(d/'nm').write_text('0 T _glGetInteger\n')
 subprocess.run(['python3',str(sweet/'gen-dispatch.py'),str(d/'layout'),str(d),str(d/'nm'),str(d/'dispatch.c')],check=True,capture_output=True)
 generated=(d/'dispatch.c').read_text();wrappers='\n'.join(l for l in generated.splitlines() if l.startswith(('static GLint w_1(','static void ps_10(')))
 assert len(wrappers.splitlines())==2
 harness='''#include <assert.h>
#include <stdint.h>
typedef int GLint;typedef unsigned GLenum;typedef unsigned GLbitfield;
static int sgl_profile_enabled,clocks,profiles,calls,binds,posts;
static uint64_t mach_absolute_time(void){clocks++;return 5;}
static void sgl_prof_add(int s,uint64_t t){assert(t==5);profiles++;}
#define SGL_CUR(x) (++binds)
#define SGL_POST(x) (++posts)
static GLint glGetInteger(GLenum x){calls++;return x+1;}
static void sgl_Clear(void *r,GLbitfield m){calls++;assert(m==7);}
'''+wrappers+'''\nint main(void){for(int e=0;e<2;e++){sgl_profile_enabled=e;clocks=profiles=0;assert(w_1(0,41)==42);ps_10(0,7);assert(clocks==2*e&&profiles==2*e);}assert(calls==4&&binds==2&&posts==2);return 0;}'''
 for name,code in [('clear',pre+clear+main),('dispatch',harness)]:
  (d/(name+'.c')).write_text(code);subprocess.run(['cc','-std=c11',str(d/(name+'.c')),'-o',str(d/name)],check=True);subprocess.run([str(d/name)],check=True)
 # Every checked-in timed wrapper must use the same opt-in gate as the generator.
 checked=(sweet/'dispatch.c').read_text()
 assert checked.count('uint64_t t0_ = sgl_profile_enabled ? mach_absolute_time() : 0;')==checked.count('sgl_prof_add(')>100
 assert checked.count('if (sgl_profile_enabled) sgl_prof_add(')==checked.count('sgl_prof_add(')
print('PASS actual clear body trace gates and GL mask/order; generated void/return slots profile on/off; checked-in wrappers gated')
