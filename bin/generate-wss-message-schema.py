"""Generate the bounded firmware domain adapter from the Relay protobuf schema.

Usage: python bin/generate-wss-message-schema.py PATH_TO_RELAY
The nanopb types remain the source for allocation-free control messages.
"""
import json
from pathlib import Path
import sys

relay = Path(sys.argv[1])
schema = json.loads((relay / 'protocol/direct-wss-schema.json').read_text())
# Query the serving codec's mapping, so there is no second kind/message mapping.
import subprocess
bodies = json.loads(subprocess.check_output(
    ['node', '-e', 'process.stdout.write(JSON.stringify(require("./lib/direct-wss-messages").bodies))'],
    cwd=relay, text=True))
names = [name for name in schema['messages'] if name != 'Envelope']
lines = ['// Generated; do not edit. Source SHA-256: ' + schema['sourceSha256'],
         '#pragma once', '#include <cstddef>', '#include <cstdint>',
         'namespace MvtWssSchema {',
         'struct Field { const char *name; uint8_t id,type,sub; bool repeated,optional; };',
         'struct Message { const Field *fields; size_t count; };']
for name in names:
    fields = schema['messages'][name]
    if not fields:
        continue
    lines.append('static const Field fields_' + name + '[] = {')
    for field in fields:
        lines.append('    {%s,%d,%d,%d,%s,%s},' % (
            json.dumps(field['name']), field['id'], field['type'],
            names.index(field['message']) if field['type'] == 11 else 0,
            str(field.get('repeated', False)).lower(), str(field.get('optional', False)).lower()))
    lines.append('};')
lines.append('static const Message messages[] = {')
for name in names:
    fields = schema['messages'][name]
    lines.append('    {%s,%d},' % ('fields_' + name if fields else 'nullptr', len(fields)))
lines.append('};')
lines.append('static const uint8_t bodies[] = {' + ','.join(str(names.index(n)) if n else '0' for n in bodies) + '};')
kinds = sorted(schema['kinds'], key=schema['kinds'].get)
lines.append('static const char *const kinds[] = {' + ','.join(json.dumps(n) for n in kinds) + '};')
lines.append('}')
target = Path(__file__).resolve().parents[1] / 'src/modules/generated/MvtWssMessageSchema.h'
target.write_text('\n'.join(lines) + '\n')
