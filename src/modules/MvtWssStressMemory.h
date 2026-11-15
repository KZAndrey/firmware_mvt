#pragma once
#if MVT_WSS_STRESS_TEST && defined(ESP_PLATFORM)
void mvtStressMemoryMark(unsigned phase, unsigned bytes=0);
void mvtStressMemoryReport();
bool mvtStressPressureStart();
void mvtStressPressureTick();
void mvtStressPressureCancel();
bool mvtStressPressureActive();
#else
inline void mvtStressMemoryMark(unsigned, unsigned=0) {}
inline void mvtStressMemoryReport() {}
inline bool mvtStressPressureStart() {return false;}
inline void mvtStressPressureTick() {}
inline void mvtStressPressureCancel() {}
inline bool mvtStressPressureActive() {return false;}
#endif
