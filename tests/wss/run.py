"""Build/run production gateway handlers using host adapters, real nanopb/cJSON.
Usage: python tests/wss/run.py --zig PATH --relay PATH
"""
import argparse
import os
from pathlib import Path
import subprocess

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
    generated = [str(p) for p in (root / 'src/mesh/generated/meshtastic').glob('*.cpp')]
    exe = build / 'gateway_test.exe'
    subprocess.run([zig, 'c++', *flags, '-std=c++11', 'tests/wss/gateway_test.cpp',
                    'src/modules/MvtWssGateway.cpp', *generated, *objects, '-o', str(exe)],
                   env=env, stdout=log, stderr=log, check=True)
subprocess.run(['node', 'tests/wss/portal-vectors.js', args.relay, str(build / 'vectors.jsonl')], check=True)
subprocess.run([str(exe), str(build / 'vectors.jsonl'), str(build / 'transcript.jsonl'), str(build)], check=True)
subprocess.run(['node', 'tests/wss/portal-contract.js', args.relay, str(build / 'transcript.jsonl')], check=True)
subprocess.run(['node', 'tests/wss/relay-integration.js', args.relay, str(build / 'transcript.jsonl')], check=True)
