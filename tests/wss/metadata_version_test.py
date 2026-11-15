"""Exercise the production metadata version assignment with the actual wire schema."""
import re
import subprocess


def run(root, build, zig, env, flags, generated, objects):
    source = (root / 'src/main.cpp').read_text(encoding='utf-8')
    body = source.split('extern meshtastic_DeviceMetadata getDeviceMetadata()', 1)[1]
    assignment = re.search(r'^\s*(?:snprintf|strncpy)\(deviceMetadata\.firmware_version,.*?;', body, re.M)
    assert assignment, 'Review metadata assignment test after refactoring getDeviceMetadata'
    cpp = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include "meshtastic/mesh.pb.h"
#include "meshtastic/deviceonly.pb.h"
#include "pb_encode.h"
bool meshtastic_NodeDatabase_callback(pb_istream_t *, pb_ostream_t *, const pb_field_t *) { assert(false); return false; }
#define optstr(x) x
static bool encode(const meshtastic_DeviceMetadata &m) {
    unsigned char bytes[256];
    auto stream=pb_ostream_from_buffer(bytes,sizeof(bytes));
    return pb_encode(&stream,meshtastic_DeviceMetadata_fields,&m);
}
int main() {
    const char *versions[]={"2.7.15", "2.7.15.1211.night", "2.7.15.1211.night.obs1311", "2.7.15.15.11", "2.7.15.15.11.rc1"};
    for(const char *APP_VERSION:versions) {
        meshtastic_DeviceMetadata deviceMetadata=meshtastic_DeviceMetadata_init_zero;
        memset(deviceMetadata.firmware_version,'X',sizeof(deviceMetadata.firmware_version));
        PRODUCTION_ASSIGNMENT
        assert(memchr(deviceMetadata.firmware_version,0,sizeof(deviceMetadata.firmware_version)));
        assert(encode(deviceMetadata));
    }
    meshtastic_DeviceMetadata old=meshtastic_DeviceMetadata_init_zero;
    strncpy(old.firmware_version,versions[2],sizeof(old.firmware_version));
    assert(!encode(old)); // reproduce the old long-version failure with real nanopb
    puts("PASS metadata: production assignment encodes short, boundary and long versions; old assignment fails");
}
'''.replace('PRODUCTION_ASSIGNMENT', assignment.group(0))
    path = build / 'metadata_version_test.cpp'
    path.write_text(cpp, encoding='utf-8')
    exe = build / 'metadata_version_test.exe'
    subprocess.run([zig, 'c++', *flags, '-std=c++11', str(path), *generated,
                    *objects, '-o', str(exe)], env=env, check=True)
    subprocess.run([str(exe)], check=True)
