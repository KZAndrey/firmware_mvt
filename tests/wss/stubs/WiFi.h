#pragma once
#define WL_CONNECTED 3
struct TestWiFi { int state = 3; bool autoReconnect=true; int status() { return state; }
    void setAutoReconnect(bool value){autoReconnect=value;}
    void disconnect(bool,bool){state=6;}
};
extern TestWiFi WiFi;
