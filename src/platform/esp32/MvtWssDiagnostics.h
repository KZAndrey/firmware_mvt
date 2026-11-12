#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// RAM only: survives socket teardown, not device reset. No credentials/payload.
typedef struct {
    uint32_t bootId, sequence, uptimeMs;
    char stage[32];
    int ret, error, socketError, tlsError, tlsCode, tlsFlags;
    int opcode, payload, offset, duringSession;
    uint32_t freeHeap, largestBlock, minimumHeap;
} MvtWssDiagnostic;
typedef struct {
    uint32_t bootId, resetReason, wakeCause, bootCount;
    uint32_t previousSleepUptimeMs, previousSleepDurationMs, sleepIntent;
} MvtWssBootDiagnostic;
void mvt_wss_boot_init(uint32_t resetReason, uint32_t wakeCause, bool retain);
void mvt_wss_prepare_sleep(uint32_t uptimeMs, uint32_t durationMs);
bool mvt_wss_boot_diagnostic(MvtWssBootDiagnostic *out);
void mvt_wss_startup_failure(const char *stage, uint32_t uptimeMs, uint32_t freeHeap,
                             uint32_t largestBlock, uint32_t minimumHeap);
bool mvt_wss_last_diagnostic(MvtWssDiagnostic *out);
// Latest failed attempt, including connects after a retained session failure.
bool mvt_wss_latest_diagnostic(MvtWssDiagnostic *out);
#ifdef __cplusplus
}
#endif
