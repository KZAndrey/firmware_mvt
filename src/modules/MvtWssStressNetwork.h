#pragma once
#if defined(ESP_PLATFORM) && MVT_WSS_STRESS_TEST
void mvtStressNetSnapshot(bool rememberPeer);
void mvtStressNetStart();
void mvtStressNetTick();
void mvtStressNetCancel();
#else
inline void mvtStressNetSnapshot(bool) {}
inline void mvtStressNetStart() {}
inline void mvtStressNetTick() {}
inline void mvtStressNetCancel() {}
#endif
