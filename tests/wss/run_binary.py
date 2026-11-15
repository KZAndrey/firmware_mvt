"""Cross-language Direct WSS binary tests; no hardware or network access."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--zig', required=True)
p.add_argument('--relay', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
os.chdir(root)
zig = str(Path(a.zig).resolve())
relay = Path(a.relay).resolve()
build = root / '.tools' / 'wss-binary-tests'
build.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ZIG_LOCAL_CACHE_DIR=str(build/'cache'), ZIG_GLOBAL_CACHE_DIR=str(build/'global-cache'))
def run(args, **kwargs):
    return subprocess.run(args, check=True, env=env, **kwargs)
run(['node', '--test', 'test_direct_wss_wire.js', 'test_direct_wss_messages.js', 'test_direct_wss_channel.js',
     'test_direct_wss_socket.js', 'test_direct_wss_integration.js'], cwd=relay)
nanopb = root / '.pio/libdeps/heltec-v3/Nanopb'
objects = []
for source in [nanopb/'pb_common.c', nanopb/'pb_encode.c', nanopb/'pb_decode.c', root/'src/modules/generated/direct-wss.pb.c']:
    obj = build/(source.stem+'.o')
    run([zig, 'cc', '-O0', '-I'+str(nanopb), '-c', str(source), '-o', str(obj)])
    objects.append(str(obj))
exe = build/'binary_nanopb_test.exe'
run([zig, 'c++', '-std=c++11', '-O0', '-Isrc', '-I'+str(nanopb),
     'tests/wss/binary_nanopb_test.cpp', *objects, '-o', str(exe)])
run([str(exe), str(build/'nanopb-gateway.bin')])
run(['node', 'scripts/test-direct-wss-nanopb.js', str(build/'nanopb-gateway.bin'), str(build/'ota.bin')], cwd=relay)
exe = build/'binary_wire_test.exe'
run([zig, 'c++', '-std=c++11', '-O0', '-Isrc', 'tests/wss/binary_wire_test.cpp', '-o', str(exe)])
with (build/'vectors.txt').open('w', encoding='utf-8') as output:
    run([str(exe), str(build/'ota.bin')], stdout=output)
run(['node', 'scripts/test-direct-wss-cross.js', str(build/'vectors.txt')], cwd=relay)

# Domain-message codec used by the production gateway, including key and signed
# manifest conversion. Keep byte equality and allocation-failure coverage here.
run(['node','scripts/test-direct-wss-domain.js','prepare',str(build/'domain.jsonl')],cwd=relay)
cjson=root/'.tools/cjson'
obj=build/'cJSON.o'
run([zig,'cc','-O0','-I'+str(cjson),'-c',str(cjson/'cJSON.c'),'-o',str(obj)])
exe=build/'domain.exe'
run([zig,'c++','-std=c++14','-O0','-Isrc','-I'+str(cjson),
     'tests/wss/binary_domain_test.cpp',str(obj),'-o',str(exe)])
run([str(exe),str(build/'domain.jsonl'),str(build/'domain.hex')])
run(['node','scripts/test-direct-wss-domain.js','verify',str(build/'domain.hex')],cwd=relay)
