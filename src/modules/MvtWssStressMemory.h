#pragma once
#if MVT_WSS_STRESS_TEST && defined(ESP_PLATFORM)
void mvtStressMemoryMark(unsigned phase, unsigned bytes=0);
void mvtStressMemoryReport();
#else
inline void mvtStressMemoryMark(unsigned, unsigned=0) {}
inline void mvtStressMemoryReport() {}
#endif
