#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <deque>
#include <string>
#include <algorithm>
#include <pb_encode.h>
#include "meshtastic/mesh.pb.h"
#include "meshtastic/telemetry.pb.h"
#include "meshtastic/deviceonly.pb.h"
#include "meshtastic/module_config.pb.h"
#define HAS_MVT_WSS_GATEWAY 1
#define APP_VERSION 2.7.15.test
#define ystr(s) #s
#define xstr(s) ystr(s)
#define LOG_WARN(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
#define NODENUM_BROADCAST UINT32_MAX
#define ERRNO_OK 0
#define RX_SRC_LOCAL 0
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
extern meshtastic_LocalConfig config;
extern meshtastic_LocalModuleConfig moduleConfig;
extern meshtastic_User owner;
inline size_t pb_encode_to_bytes(uint8_t *dest, size_t capacity, const pb_msgdesc_t *fields, const void *value) {
    pb_ostream_t s = pb_ostream_from_buffer(dest, capacity);
    return pb_encode(&s, fields, value) ? s.bytes_written : 0;
}
struct Pool { void release(meshtastic_MeshPacket *p) { delete p; } };
extern Pool packetPool;
struct Channels { uint8_t getPrimaryIndex() const { return 0; } const char *getName(size_t) const { return "LongFast"; } };
extern Channels channels;
