#pragma once
#include <cstdint>
#include <cstdio>
#define HAS_MVT_WSS_GATEWAY 1
#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1
#define APP_VERSION ota_host_test
#define MVT_STR(x) #x
#define xstr(x) MVT_STR(x)
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
struct TestModuleConfig { struct { char username[64] = "ROOM1"; } mqtt; };
extern TestModuleConfig moduleConfig;
