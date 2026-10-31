#pragma once
#include "MvtWssDiagnostics.h"
#include <cJSON.h>
static inline cJSON *mvtWssDiagnosticJson()
{
    MvtWssDiagnostic d;
    if (!mvt_wss_last_diagnostic(&d)) return nullptr;
    cJSON *p = cJSON_CreateObject();
    if (!p) return nullptr;
    cJSON_AddStringToObject(p, "stage", d.stage);
#define MVT_DIAG_NUMBER(name) cJSON_AddNumberToObject(p, #name, d.name)
    MVT_DIAG_NUMBER(bootId); MVT_DIAG_NUMBER(sequence); MVT_DIAG_NUMBER(uptimeMs);
    MVT_DIAG_NUMBER(ret); MVT_DIAG_NUMBER(error); MVT_DIAG_NUMBER(socketError);
    MVT_DIAG_NUMBER(tlsError); MVT_DIAG_NUMBER(tlsCode); MVT_DIAG_NUMBER(tlsFlags);
    MVT_DIAG_NUMBER(opcode); MVT_DIAG_NUMBER(payload); MVT_DIAG_NUMBER(offset);
    MVT_DIAG_NUMBER(duringSession);
#undef MVT_DIAG_NUMBER
    return p;
}
