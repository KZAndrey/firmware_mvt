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
} MvtWssDiagnostic;
bool mvt_wss_last_diagnostic(MvtWssDiagnostic *out);
#ifdef __cplusplus
}
#endif
