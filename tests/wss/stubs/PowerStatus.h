#pragma once
struct PowerStatus {
    uint32_t getBatteryChargePercent() { return 85; }
    uint32_t getBatteryVoltageMv() { return 4100; }
};
extern PowerStatus *powerStatus;
