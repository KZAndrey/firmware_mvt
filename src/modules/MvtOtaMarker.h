#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Bounded streaming KMP matcher. Image bytes can contain NUL and the version
// marker can cross chunk boundaries. Feeding chunks never allocates heap.
class MvtOtaMarker {
public:
    MvtOtaMarker(const char *pattern, size_t length) : length(length) {
        if (!pattern || !length || length > sizeof(bytes)) { this->length = 0; return; }
        memcpy(bytes, pattern, length);
        for (size_t i = 1, j = 0; i < length; ++i) {
            while (j && bytes[i] != bytes[j]) j = prefix[j - 1];
            if (bytes[i] == bytes[j]) ++j;
            prefix[i] = uint8_t(j);
        }
    }
    bool valid() const { return length != 0; }
    bool feed(const uint8_t *data, size_t count) {
        if (found || !length) return found;
        for (size_t i = 0; i < count; ++i) {
            while (matched && data[i] != bytes[matched]) matched = prefix[matched - 1];
            if (data[i] == bytes[matched]) ++matched;
            if (matched == length) { found = true; break; }
        }
        return found;
    }
private:
    uint8_t bytes[96] = {}, prefix[96] = {};
    size_t length = 0, matched = 0;
    bool found = false;
};
