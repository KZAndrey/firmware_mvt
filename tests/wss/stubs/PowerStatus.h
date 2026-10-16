#pragma once
struct PowerStatus {
    bool getHasUSB(){return true;}
    bool getHasBattery(){return true;}
    bool getIsCharging(){return true;}
    uint32_t getBatteryChargePercent() { return 85; }
    uint32_t getBatteryVoltageMv() { return 4100; }
};
extern PowerStatus *powerStatus;
