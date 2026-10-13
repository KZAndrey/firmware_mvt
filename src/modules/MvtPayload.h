#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cmath>

// Same wire layout as TrackMooov and MVT_Relay/lib/meshtastic-mqtt.js.
namespace mvt {
static const char *const colors[] = {"RED", "ORG", "YLW", "GRN", "BLU", "LAV", "WHT", "PNK", "GRY"};
static const char *const icons[] = {"WLK", "BIK", "CAR", "CMP", "RAD", "TGT"};
struct Payload {
    double lat = 0, lon = 0;
    uint8_t battery = 0, color = 0, icon = 0, flags = 0;
    bool charging = false;
    int16_t altitude = 0;
    uint8_t heartRate = 0;
    std::string name, comment;
};
inline uint32_t read32(const uint8_t *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
inline void write32(uint8_t *p, int32_t n) {
    uint32_t u = uint32_t(n);
    for (int i = 3; i >= 0; --i) { p[i] = u & 255; u >>= 8; }
}
inline bool decode(const uint8_t *p, size_t n, Payload &v) {
    if (!p || n < 13 || p[0] != 'M' || p[1] != 'V') return false;
    v = Payload();
    v.lat = int32_t(read32(p + 2)) / 1e6;
    v.lon = int32_t(read32(p + 6)) / 1e6;
    if (v.lat < -90 || v.lat > 90 || v.lon < -180 || v.lon > 180) return false;
    v.battery = p[10] & 127; v.charging = p[10] & 128;
    v.color = p[11] >> 4; v.icon = p[11] & 15; v.flags = p[12];
    size_t offset = 13;
    if (v.flags & 16) {
        if (n < offset + 2) return false;
        v.altitude = int16_t((uint16_t(p[offset]) << 8) | p[offset + 1]); offset += 2;
    }
    if (v.flags & 32) {
        if (n < offset + 1) return false;
        v.heartRate = p[offset++];
    }
    std::string text(reinterpret_cast<const char *>(p + offset), n - offset);
    while (!text.empty() && text.back() == '\0') text.pop_back();
    size_t split = text.find('\n');
    v.name = text.substr(0, split);
    if (split != std::string::npos) v.comment = text.substr(split + 1);
    return true;
}
inline size_t encode(const Payload &v, uint8_t *p, size_t capacity) {
    if (!p || !std::isfinite(v.lat) || !std::isfinite(v.lon) ||
        v.lat < -90 || v.lat > 90 || v.lon < -180 || v.lon > 180) return 0;
    std::string text = v.name + (v.comment.empty() ? "" : "\n" + v.comment);
    size_t n = 13 + ((v.flags & 16) ? 2 : 0) + ((v.flags & 32) ? 1 : 0) + text.size();
    if (n > capacity || n > 233) return 0; // Reject; never truncate UTF-8.
    p[0] = 'M'; p[1] = 'V';
    write32(p + 2, int32_t(round(v.lat * 1e6)));
    write32(p + 6, int32_t(round(v.lon * 1e6)));
    p[10] = (v.battery > 100 ? 100 : v.battery) | (v.charging ? 128 : 0);
    p[11] = (v.color << 4) | (v.icon & 15); p[12] = v.flags;
    size_t offset = 13;
    if (v.flags & 16) { p[offset++] = uint16_t(v.altitude) >> 8; p[offset++] = uint16_t(v.altitude) & 255; }
    if (v.flags & 32) p[offset++] = v.heartRate;
    text.copy(reinterpret_cast<char *>(p + offset), text.size());
    return n;
}
}
