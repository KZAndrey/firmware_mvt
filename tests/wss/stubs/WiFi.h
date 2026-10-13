#pragma once
#define WL_CONNECTED 3
struct TestWiFi { int state = 3; int status() { return state; } };
extern TestWiFi WiFi;
