#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uintptr_t vm_offset_t;
static uint32_t auxiliary[256], ring[256], ipc_words[4];
static unsigned fifo_reads, writes;
static uint32_t ri_rd(vm_offset_t base, unsigned offset)
{
 assert(!(offset & 3));
 if (base == (uintptr_t)ring) { assert(offset < sizeof(ring)); fifo_reads++; }
 else assert(base == (uintptr_t)auxiliary && offset < sizeof(auxiliary));
 return ((uint32_t *)base)[offset/4];
}
static void ri_wr(vm_offset_t base, unsigned offset, uint32_t value)
{
 assert((base == (uintptr_t)auxiliary && (offset == 80 || offset == 68)) ||
        (base == (uintptr_t)ipc_words && offset == 0));
 writes++; ((uint32_t *)base)[offset/4] = value;
}
static void host_barrier(void) { }
#include "rpm-reply-actual.h"

static void reset(unsigned tail)
{
 memset(auxiliary, 0, sizeof(auxiliary));
 memset(ring, 0xaa, sizeof(ring));
 memset(ipc_words, 0, sizeof(ipc_words));
 auxiliary[0] = auxiliary[44/4] = 2;
 auxiliary[80/4] = auxiliary[84/4] = tail;
 fifo_reads = writes = 0;
}
static unsigned append(unsigned start, const uint32_t *payload, unsigned bytes)
{
 ring[start/4] = bytes;
 for (unsigned i = 4; i < 20; i += 4) ring[((start+i)&1023)/4] = 0;
 for (unsigned i = 0; i < bytes; i += 4)
  ring[((start+20+i)&1023)/4] = payload[i/4];
 unsigned end = (start+20+((bytes+3)&~3u))&1023;
 auxiliary[84/4] = end; return end;
}
static int take(uint32_t wanted, uint32_t *id)
{
 return ri_rpm_take_reply((uintptr_t)auxiliary, (uintptr_t)ring,
     (uintptr_t)ipc_words, wanted, id);
}
static void assert_unchanged(int expected, uint32_t wanted)
{
 uint32_t before[256], fifo_before[256], id = 123;
 memcpy(before, auxiliary, sizeof(before));
 memcpy(fifo_before, ring, sizeof(fifo_before));
 assert(take(wanted, &id) == expected);
 assert(!memcmp(before, auxiliary, sizeof(before)));
 assert(!memcmp(fifo_before, ring, sizeof(fifo_before)));
 assert(writes == 0 && ipc_words[0] == 0);
}
int main(void)
{
 uint32_t ack[32] = {0x716572,12,0x2367736d,4,42};
 uint32_t id = 0, before[256], fifo_before[256];
 reset(0); append(0, ack, 20);
 memcpy(before, auxiliary, sizeof(before)); memcpy(fifo_before, ring, sizeof(ring));
 assert(take(42, &id) == RI_RPM_REPLY_ACK && id == 42);
 assert(auxiliary[80/4] == 40 && auxiliary[68/4] == 1 && ipc_words[0] == 1);
 before[80/4] = 40; before[68/4] = 1;
 assert(!memcmp(before, auxiliary, sizeof(before)));
 assert(!memcmp(fifo_before, ring, sizeof(fifo_before)));
 assert(writes == 3);

 /* The live boot bug: duplicate ACK1 must not acknowledge request2. */
 reset(0); ack[4] = 1; unsigned end = append(0, ack, 20);
 end = append(end, ack, 20); ack[4] = 2; append(end, ack, 20);
 assert(take(2, &id) == RI_RPM_REPLY_STALE && id == 1);
 assert(take(2, &id) == RI_RPM_REPLY_STALE && id == 1);
 assert(take(2, &id) == RI_RPM_REPLY_ACK && id == 2);
 assert(auxiliary[80/4] == 120);

 uint32_t nack[32] = {0x716572,24,0x2367736d,4,42,0x727265,4,0x626164};
 reset(0); append(0, nack, 32);
 assert(take(42, &id) == RI_RPM_REPLY_NACK && id == 42);
 reset(0); append(0, nack, 32);
 assert(take(43, &id) == RI_RPM_REPLY_STALE && id == 42);

 /* Both payload and SMD header wrap at the FIFO boundary. */
 ack[4] = 42; reset(1008); append(1008, ack, 20);
 assert(take(42, &id) == RI_RPM_REPLY_ACK && auxiliary[80/4] == 24);

 reset(0); assert_unchanged(RI_RPM_REPLY_WAIT,42); assert(fifo_reads == 0);
 reset(0); append(0,ack,20); auxiliary[84/4] = 16;
 assert_unchanged(RI_RPM_REPLY_WAIT,42); assert(fifo_reads == 0);
 reset(0); append(0,ack,20); auxiliary[84/4] = 36;
 assert_unchanged(RI_RPM_REPLY_WAIT,42); assert(fifo_reads == 1);

 for (unsigned field = 0; field < 5; field++) {
  uint32_t bad[32]; memcpy(bad,ack,sizeof(bad)); bad[field] = 0;
  reset(0); append(0,bad,20); assert_unchanged(RI_RPM_REPLY_BAD,42);
 }
 for (unsigned length = 0; length <= 132; length += 4) {
  if (length == 20) continue;
  reset(0); ring[0] = length; auxiliary[84/4] = 20;
  assert_unchanged(length >= 20 && length <= 128 ? RI_RPM_REPLY_WAIT : RI_RPM_REPLY_BAD,42);
 }
 /* Complete but inconsistent or unknown trailing payload is not success. */
 uint32_t bad[32]; memcpy(bad,nack,sizeof(bad)); bad[6] = 99;
 reset(0); append(0,bad,32); assert_unchanged(RI_RPM_REPLY_BAD,42);
 memcpy(bad,nack,sizeof(bad)); bad[5] = 0xdead;
 reset(0); append(0,bad,32); assert_unchanged(RI_RPM_REPLY_BAD,42);
 reset(0); append(0,ack,20); auxiliary[0] = 0;
 assert_unchanged(RI_RPM_REPLY_BAD,42); assert(fifo_reads == 0);
 reset(0); append(0,ack,20); auxiliary[44/4] = 3;
 assert_unchanged(RI_RPM_REPLY_BAD,42); assert(fifo_reads == 0);
 for (unsigned i = 0; i < 2; i++) {
  for (unsigned j = 0; j < 2; j++) {
   reset(0); auxiliary[(i?80:84)/4] = j?1024:1;
   assert_unchanged(RI_RPM_REPLY_BAD,42); assert(fifo_reads == 0);
  }
 }
 reset(0); append(0,ack,20); assert_unchanged(RI_RPM_REPLY_BAD,0);
 assert(take(42,NULL) == RI_RPM_REPLY_BAD);
 puts("exact RPM IDs, duplicate/stale ACKs, NACKs, partial/wrapped/malformed frames and bounded writes: PASS");
 return 0;
}
