#pragma once

#include "concurrency/Periodic.h"
#include "configuration.h"
#include <Arduino.h>
#include <functional>

#if HAS_WIFI && !defined(ARCH_PORTDUINO)
#include <WiFi.h>
#endif

#if HAS_ETHERNET && defined(USE_WS5500)
#include <ETHClass2.h>
#define ETH ETH2
#endif // HAS_ETHERNET

extern bool needReconnect;
extern concurrency::Periodic *wifiReconnect;

/// @return true if wifi is now in use
bool initWifi();

void deinitWifi();

bool isWifiAvailable();

// Schedule the existing non-blocking rejoin path without changing mesh config.
bool requestWifiReconnect();

uint8_t getWifiDisconnectReason();

#ifdef USE_WS5500
// Startup Ethernet
bool initEthernet();
#endif
