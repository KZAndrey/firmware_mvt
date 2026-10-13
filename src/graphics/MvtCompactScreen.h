#pragma once
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <cstring>
#include <cmath>
#include "assets/MovetainLogo.h"

namespace mvtScreen {
inline std::string statusLine(const char *status) {
    const char *code = status ? status : "OFF";
    if (!strcmp(code,"BADKEY")) return "BAD KEY";
    if (!strcmp(code,"NOROOM")) return "NO ROOM";
    if (!strcmp(code,"TAKEN")) return "OTHER GW";
    if (!strcmp(code,"BLOCK")) return "BLOCKED";
    if (!strcmp(code,"BADURL")) return "BAD URL";
    if (!strcmp(code,"FAIL")) return "AUTH ERROR";
    if (!strcmp(code,"FRAME")) return "DATA ERROR";
    if (!strcmp(code,"SETUP")) return "SET ROOM";
    if (!strcmp(code,"SETKEY")) return "SET KEY";
    if (!strcmp(code,"WAIT")) return "NO WIFI";
    if (!strcmp(code,"RETRY")) return "WSS:RETRY";
    return std::string("WSS:") + code;
}
// Fixed 5x7 ASCII glyphs; a 6px cell fits ten characters on a 64px OLED.
inline const uint8_t *glyph(char ch) {
    static const uint8_t digits[10][5] = {
        {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},
        {0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
        {0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e}};
    static const uint8_t letters[26][5] = {
        {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},
        {0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},
        {0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},
        {0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
        {0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
        {0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},{0x3f,0x40,0x40,0x40,0x3f},
        {0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},
        {7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}};
    static const uint8_t blank[5] = {}, colon[5] = {0,0x36,0x36,0,0}, dash[5] = {8,8,8,8,8};
    static const uint8_t percent[5] = {0x63,0x13,8,0x64,0x63}, under[5] = {0x40,0x40,0x40,0x40,0x40};
    static const uint8_t question[5] = {2,1,0x51,9,6};
    static const uint8_t dot[5] = {0,0x60,0x60,0,0};
    if (ch >= '0' && ch <= '9') return digits[ch-'0'];
    ch = char(toupper(static_cast<unsigned char>(ch)));
    if (ch >= 'A' && ch <= 'Z') return letters[ch-'A'];
    if (ch == ':') return colon;
    if (ch == '-') return dash;
    if (ch == '%') return percent;
    if (ch == '_') return under;
    if (ch == '.') return dot;
    return ch == ' ' ? blank : question;
}
inline std::string roomLine(const char *room, uint32_t tick) {
    std::string value = room && room[0] ? room : "--";
    if (value.size() <= 8) return "R:" + value;
    value += "   ";
    size_t start = tick / 700 % value.size();
    return "R:" + (value + value).substr(start, 8);
}
inline std::string networkName(const char *ssid, uint32_t tick) {
    std::string value = ssid && ssid[0] ? ssid : "NO WIFI";
    if (value.size() <= 10) return value;
    value += "   ";
    size_t start = tick / 700 % value.size();
    return (value + value).substr(start, 10);
}
inline std::string presetLine(const char *name) {
    std::string value = name ? name : "UNKNOWN";
    if (value == "VERY_LONG_SLOW") return "V LONGSLOW";
    if (value == "MEDIUM_SLOW") return "MED SLOW";
    if (value == "MEDIUM_FAST") return "MED FAST";
    if (value == "LONG_MODERATE") return "LONG MOD";
    if (value == "SHORT_TURBO") return "SHT TURBO";
    for (char &ch : value) if (ch == '_') ch = ' ';
    return value;
}
inline std::string frequencyLine(double mhz) {
    if (!std::isfinite(mhz) || mhz <= 0) return "FREQ:--";
    char value[11];
    snprintf(value, sizeof(value), mhz >= 1000 ? "%.2fMHZ" : "%.3fMHZ", mhz);
    return value;
}
inline std::string memoryLine(const char *label, uint32_t bytes) {
    char value[11];
    snprintf(value, sizeof(value), "%s:%luK", label, static_cast<unsigned long>(bytes / 1024));
    return value;
}
inline bool alternatePhase(uint64_t seconds) { return (seconds / 5) % 2 != 0; }
inline std::string voltageLine(int millivolts) {
    if (millivolts <= 0 || millivolts > 9999) return "VLT:--";
    char value[11];
    snprintf(value, sizeof(value), "VLT:%d.%02dV", millivolts / 1000, (millivolts % 1000) / 10);
    return value;
}
inline std::string uptimeLine(uint64_t seconds) {
    char value[24];
    const auto days = seconds / 86400;
    if (days)
        snprintf(value, sizeof(value), "%lluD %02u:%02u", static_cast<unsigned long long>(days),
                 unsigned(seconds / 3600 % 24), unsigned(seconds / 60 % 60));
    else
        snprintf(value, sizeof(value), "%02u:%02u", unsigned(seconds / 3600), unsigned(seconds / 60 % 60));
    return value;
}
template<class Display> void clockLogo(Display *display, int x, int y) {
    // Sample only the mountain mark from the portal raster, without the wordmark.
    for (int py=0;py<14;++py) for (int px=0;px<24;++px) {
        int sx=11+px*42/24, sy=py*24/14;
        if (logoBits[sy*8+sx/8] & (1<<(sx%8))) display->setPixel(x+px,y+py);
    }
}
inline std::string count(uint32_t value) {
    char b[8];
    if (value < 1000) snprintf(b,sizeof(b),"%u",unsigned(value));
    else if (value < 100000) snprintf(b,sizeof(b),"%uK",unsigned(value/1000));
    else if (value >= 1000000 && value < 100000000) snprintf(b,sizeof(b),"%uM",unsigned(value/1000000));
    else snprintf(b,sizeof(b),"HI");
    return std::string(b);
}
template<class Display> void row(Display *display, int x, int y, const std::string &text) {
    for (size_t i=0; i<text.size() && i<10; ++i) {
        const uint8_t *g = glyph(text[i]);
        for (int col=0; col<5; ++col) for (int bit=0; bit<7; ++bit) {
            int px = x+2+int(i)*6+col, py=y+bit;
            if ((g[col] & (1<<bit)) && px>=0 && px<64 && py>=0 && py<32) display->setPixel(px,py);
        }
    }
}
}
