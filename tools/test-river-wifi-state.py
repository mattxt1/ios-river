#!/usr/bin/env python3
"""Compile actual patch0289 state transitions with deterministic hardware stubs.
Pass a kernel tree with patches through0288 applied (before0289). No phone access.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--kernel-source',type=Path,required=True)
p.add_argument('--extra-patch',type=Path,action='append',default=[],help='Additional patch to apply after0289 (repeatable).')
a=p.parse_args()
r=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='river-wifi-state-') as tmp:
    d=Path(tmp)
    for rel in ('bsd/dev/sweet_keys.c','bsd/dev/river_apple80211.c','bsd/sys/river_apple80211.h'):
        target=d/rel;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((a.kernel_source/rel).read_bytes())
    subprocess.run(['git','apply',str(r/'xnu/patches/0289-river-wifi-connection-state.patch')],cwd=d,check=True)
    for extra in a.extra_patch:
        subprocess.run(['git','apply',str(extra.resolve())],cwd=d,check=True)
    abi=(d/'bsd/sys/river_apple80211.h').read_text()
    abi='\n'.join(line for line in abi.splitlines() if not line.startswith('#include'))
    (d/'river-wifi-abi.h').write_text(abi)
    engine=(d/'bsd/dev/river_apple80211.c').read_text()
    engine='\n'.join(line for line in engine.splitlines() if not line.startswith('#include'))
    start=engine.index('static void\nhmac_sha1(');end=engine.index('static void\npost(',start)
    engine=engine[:start]+engine[end:]
    has_result_abi='RIVER_ASSOC_SUCCESS' in engine
    if has_result_abi:
        engine='#define HOST_RESULT_ABI 1\n'+engine
    if 'RIVER_BSS_MAX_AGE_MS' in engine:
        engine='#define HOST_FRESH_SCAN 1\n'+engine
    if 'river_network_link_events(' in (d/'bsd/dev/sweet_keys.c').read_text():
        engine='#define HOST_LINK_EVENTS 1\n'+engine
    (d/'river-wifi-engine.h').write_text(engine)
    driver=(d/'bsd/dev/sweet_keys.c').read_text()
    if 'river_probe_tx(const struct river_apple80211_join_req *name)' in driver:
        start=driver.index('static bool\nriver_probe_tx(');end=driver.index('/* Single outstanding HAL',start)
        (d/'river-wifi-probe.h').write_text(driver[start:end])
        scan=driver[driver.index('static void\nriver_passive_scan('):driver.index('static void\nriver_wcnss_probe(')]
        assert scan.index('if (river_wpa.stage == 2 && river_target.found) return;')<scan.index('river_apple80211_scan_begin(')
        assert 'river_probe_tx(directed)' in scan and 'ch <= 11' in scan
    start=driver.index('static void\nriver_target_rx(');end=driver.index('static uint32_t\nriver_bd_word',start)
    rx=driver[start:end]
    assert 'river_scan_xact(' not in rx and 'river_dxe_poll(' not in rx
    (d/'river-wifi-rx.h').write_text(rx)
    start=driver.index('static void\nriver_connection_cleanup(');end=driver.index('static void\nriver_wcnss_thread(',start)
    (d/'river-wifi-worker.h').write_text(driver[start:end])
    poll=driver[driver.index('static void\nriver_wcnss_thread('):driver.index('static void\nriver_gpu_probe(')]
    assert poll.index('river_connection_service();')<poll.index('river_network_poll();')
    if 'river_network_link_events();' in poll:
        assert poll.index('river_connection_service();')<poll.index('river_network_link_events();')<poll.index('river_network_poll();')
        assert 'ifnet_event(' not in rx
    binary=d/'wifi-state'
    subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-I',tmp,
                    str(r/'tools/tests/river-wifi-state-host.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
