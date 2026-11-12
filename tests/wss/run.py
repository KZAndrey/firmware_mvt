"""Build/run production gateway handlers using host adapters, real nanopb/cJSON.
Usage: python tests/wss/run.py --zig PATH --relay PATH
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--zig', required=True)
parser.add_argument('--relay', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
os.chdir(root)
zig = str(Path(args.zig).resolve())
build = root / '.tools' / 'wss-tests'
build.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ZIG_LOCAL_CACHE_DIR=str(build / 'cache'), ZIG_GLOBAL_CACHE_DIR=str(build / 'global-cache'))
subprocess.run([sys.executable,'tests/wss/usb_heap_guard_test.py'],check=True)
from nodedb_capacity_test import run as test_nodedb_capacity
test_nodedb_capacity(root, build, zig, env)
from mqtt_proxy_test import run as test_mqtt_proxy
test_mqtt_proxy(root, build, zig, env)
memory_trace=build/'memory_trace_test.exe'
subprocess.run([zig,'c++','-std=c++14','-O0','tests/wss/memory_trace_test.cpp','-o',str(memory_trace)],env=env,check=True)
subprocess.run([str(memory_trace)],check=True)
tx_probe=build/'stress_wifi_tx_test.exe'
subprocess.run([zig,'c++','-std=c++14','-O0','-Itests/wss/tx-probe-stubs','tests/wss/stress_wifi_tx_test.cpp','-o',str(tx_probe)],env=env,check=True)
subprocess.run([str(tx_probe)],check=True)
heap_pool=build/'bounded_heap_pool_test.exe'
subprocess.run([zig,'c++','-std=c++14','-O0','tests/wss/bounded_heap_pool_test.cpp','-o',str(heap_pool)],env=env,check=True)
subprocess.run([str(heap_pool)],check=True)
rx_buffer=build/'rx_buffer_test.exe'
subprocess.run([zig,'c++','-std=c++14','-O0','-fno-exceptions','tests/wss/rx_buffer_test.cpp','-o',str(rx_buffer)],env=env,check=True)
subprocess.run([str(rx_buffer)],check=True)
wifi_config=build/'stress_wifi_config_test.exe'
subprocess.run([zig,'c++','-std=c++14','-O0','-Itests/wss/tx-probe-stubs','-Isrc','tests/wss/stress_wifi_config_test.cpp','-o',str(wifi_config)],env=env,check=True)
subprocess.run([str(wifi_config)],check=True)
wifi_trace=build/'stress_wifi_trace_test.exe'
subprocess.run([zig,'c++','-std=c++14','-O0','tests/wss/stress_wifi_trace_test.cpp','-o',str(wifi_trace)],env=env,check=True)
subprocess.run([str(wifi_trace)],check=True)
alloc_test=build/'stress_allocations_test.exe'
subprocess.run([zig,'c++','-std=c++14','-O0','tests/wss/stress_allocations_test.cpp','-o',str(alloc_test)],env=env,check=True)
subprocess.run([str(alloc_test)],check=True)
stress_test=build/'stress_control_test.exe'
subprocess.run([zig,'c++','-std=c++14','-O0','tests/wss/stress_control_test.cpp','-o',str(stress_test)],env=env,check=True)
subprocess.run([str(stress_test)],check=True)
send_test = build / 'websocket_send_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', 'tests/wss/websocket_send_test.cpp', '-o', str(send_test)], env=env, check=True)
subprocess.run([str(send_test)], check=True)
diag_test = build / 'websocket_diagnostics_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', 'tests/wss/websocket_diagnostics_test.cpp', '-o', str(diag_test)], env=env, check=True)
subprocess.run([str(diag_test)], check=True)
worker_test = build / 'transport_worker_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', '-DHAS_MVT_WSS_GATEWAY=1', '-Itests/wss/worker-stubs', '-Isrc', 'tests/wss/transport_worker_test.cpp', 'src/platform/esp32/MvtWssTransport.cpp', '-o', str(worker_test)], env=env, check=True)
subprocess.run([str(worker_test)], check=True)
tls_test = build / 'tls_buffer_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', '-Itests/tls-memory/stubs', '-Isrc',
                'tests/tls-memory/buffer_test.cpp', 'src/platform/esp32/MvtTlsMemory.cpp',
                '-o', str(tls_test)], env=env, check=True)
subprocess.run([str(tls_test)], check=True)
poll_test = build / 'websocket_poll_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', 'tests/wss/websocket_poll_test.cpp',
                '-o', str(poll_test)], env=env, check=True)
subprocess.run([str(poll_test)], check=True)
control_test = build / 'websocket_control_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', 'tests/wss/websocket_control_test.cpp', '-o', str(control_test)], env=env, check=True)
subprocess.run([str(control_test)], check=True)
receive_test = build / 'websocket_receive_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', 'tests/wss/websocket_receive_test.cpp',
                '-o', str(receive_test)], env=env, check=True)
subprocess.run([str(receive_test)], check=True)
marker_test = build / 'ota_marker_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', 'tests/wss/ota_marker_test.cpp',
                '-o', str(marker_test)], env=env, check=True)
subprocess.run([str(marker_test)], check=True)
recovery_test = build / 'network_recovery_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', 'tests/wss/network_recovery_test.cpp',
                '-o', str(recovery_test)], env=env, check=True)
subprocess.run([str(recovery_test)], check=True)
tls_read_test = build / 'tls_read_retry_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', 'tests/wss/tls_read_retry_test.cpp', '-o', str(tls_read_test)], env=env, check=True)
subprocess.run([str(tls_read_test)], check=True)
tls_routing_test = build / 'tls_read_routing_test.exe'
subprocess.run([zig, 'c++', '-std=c++14', '-O0', '-DHAS_MVT_WSS_GATEWAY=1', '-Itests/tls-read/stubs', '-Isrc', 'tests/tls-read/routing_test.cpp', 'src/platform/esp32/MvtTlsRead.cpp', '-o', str(tls_routing_test)], env=env, check=True)
subprocess.run([str(tls_routing_test)], check=True)
nanopb = root / '.pio/libdeps/heltec-v3/Nanopb'
cjson = root / '.tools/cjson'
if not (cjson / 'cJSON.c').exists():
    raise SystemExit('Place cJSON.c and cJSON.h v1.7.19 in .tools/cjson before running.')
flags = ['-target', 'x86_64-windows-gnu', '-DPB_ENABLE_MALLOC=1', '-O0', '-g0',
         '-Itests/wss/stubs', '-Isrc', '-Isrc/mesh/generated', '-I' + str(nanopb), '-I' + str(cjson)]
with (build / 'compile.log').open('w', encoding='utf-8') as log:
    objects = []
    for src in [nanopb / 'pb_common.c', nanopb / 'pb_decode.c', nanopb / 'pb_encode.c', cjson / 'cJSON.c']:
        obj = build / (src.stem + '.o')
        subprocess.run([zig, 'cc', *flags, '-c', str(src), '-o', str(obj)], env=env, stdout=log, stderr=log, check=True)
        objects.append(str(obj))
    cancellation_test = build / 'ota_cancellation_test.exe'
    diagnostic_wire = build / 'diagnostic_wire_test.exe'
    subprocess.run([zig,'c++','-std=c++14','-O0','-Isrc','-I'+str(cjson),
                    'tests/wss/diagnostic_wire_test.cpp',str(build/'cJSON.o'),'-o',str(diagnostic_wire)],
                   env=env,stdout=log,stderr=log,check=True)
    subprocess.run([str(diagnostic_wire)],check=True)
    subprocess.run([zig, 'c++', '-target', 'x86_64-windows-gnu', '-O0', '-g0',
                    '-Itests/ota/stubs', '-Isrc', '-I' + str(cjson), '-std=c++14',
                    'tests/ota/cancellation_test.cpp', 'src/modules/MvtOtaUpdater.cpp',
                    str(build / 'cJSON.o'), '-o', str(cancellation_test)],
                   env=env, stdout=log, stderr=log, check=True)
    generated = [str(p) for p in (root / 'src/mesh/generated/meshtastic').glob('*.cpp')]
    exe = build / 'gateway_test.exe'
    subprocess.run([zig, 'c++', *flags, '-std=c++11', 'tests/wss/gateway_test.cpp',
                    'src/modules/MvtWssGateway.cpp', *generated, *objects, '-o', str(exe)],
                   env=env, stdout=log, stderr=log, check=True)
    stress_gateway=build/'stress_gateway_test.exe'
    subprocess.run([zig,'c++',*flags,'-std=c++11','-DMVT_WSS_STRESS_TEST=1','tests/wss/gateway_test.cpp',
                    'src/modules/MvtWssGateway.cpp','src/modules/MvtWssStressControl.cpp',*generated,*objects,'-o',str(stress_gateway)],
                   env=env,stdout=log,stderr=log,check=True)
subprocess.run([str(cancellation_test)], check=True)
subprocess.run(['node', 'tests/wss/portal-vectors.js', args.relay, str(build / 'vectors.jsonl')], check=True)
subprocess.run([str(exe), str(build / 'vectors.jsonl'), str(build / 'transcript.jsonl'), str(build)], check=True)
subprocess.run([str(stress_gateway), str(build / 'vectors.jsonl'), str(build / 'stress-transcript.jsonl'), str(build)], check=True)
subprocess.run(['node', 'tests/wss/portal-contract.js', args.relay, str(build / 'transcript.jsonl')], check=True)
subprocess.run(['node', 'tests/wss/relay-integration.js', args.relay, str(build / 'transcript.jsonl')], check=True)
