#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY && MVT_WSS_STRESS_TEST
#include "MvtWssStressControl.h"
std::atomic<unsigned> mvtStressMailbox{0};
std::atomic<uint32_t> mvtStressWifiUntil{0};
std::atomic<bool> mvtStressRadioReset{false};
std::atomic<unsigned> mvtStressTxLimit{0};
#endif
