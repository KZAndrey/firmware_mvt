#pragma once
#include "MvtWssDiagnostics.h"
#include <cJSON.h>
#include <cstdio>
// Fixed, bounded wire representation: no heap-backed tree of numeric leaves.
// cJSON_CreateRaw copies the completed buffer; nothing refers to this stack.
class MvtDiagnosticWire {
    char data[768]={'{',0};
    size_t used=1;
    bool valid=true;
public:
    template<class... A> void append(const char *format,A... args) {
        if(!valid)return;
        const int n=std::snprintf(data+used,sizeof(data)-used,format,args...);
        if(n<0||size_t(n)>=sizeof(data)-used){valid=false;return;}
        used+=size_t(n);
    }
    // All protocol fields are 32-bit. nano.specs does not promise long-long
    // printf support; cast explicitly to the matching signed/unsigned long.
    void number(const char *name,uint32_t value){append("\"%s\":%lu,",name,static_cast<unsigned long>(value));}
    void number(const char *name,int32_t value){append("\"%s\":%ld,",name,static_cast<long>(value));}
    void stage(const char (&value)[32]) {
        append("%s","\"stage\":\"");
        for(unsigned i=0;i<sizeof(value)&&value[i];++i){
            const unsigned char c=value[i];
            if(c=='"'||c=='\\')append("\\%c",int(c));
            else if(c<32)append("\\u%04x",unsigned(c));
            else append("%c",int(c));
        }
        append("%s","\",");
    }
    cJSON *finish(){
        if(!valid)return nullptr;
        if(used>1&&data[used-1]==',')--used;
        append("%s","}");
        return valid?cJSON_CreateRaw(data):nullptr;
    }
};
// Only literal field names are passed here. Keep them in read-only storage
// instead of fragmenting the heap with dozens of identical key copies.
static inline void mvtWssDiagnosticField(cJSON *parent, const char *literal, cJSON *child) {
    if(child&&!cJSON_AddItemToObjectCS(parent,literal,child))cJSON_Delete(child);
}
static inline cJSON *mvtWssDiagnosticJson(bool latest = false, bool wire = false)
{
    MvtWssDiagnostic d;
    if (!(latest ? mvt_wss_latest_diagnostic(&d) : mvt_wss_last_diagnostic(&d))) return nullptr;
    if(wire){
        MvtDiagnosticWire w;w.stage(d.stage);
#define MVT_WIRE_NUMBER(name) w.number(#name,d.name)
        MVT_WIRE_NUMBER(bootId);MVT_WIRE_NUMBER(sequence);MVT_WIRE_NUMBER(uptimeMs);
        MVT_WIRE_NUMBER(ret);MVT_WIRE_NUMBER(error);MVT_WIRE_NUMBER(socketError);
        MVT_WIRE_NUMBER(tlsError);MVT_WIRE_NUMBER(tlsCode);MVT_WIRE_NUMBER(tlsFlags);
        MVT_WIRE_NUMBER(opcode);MVT_WIRE_NUMBER(payload);MVT_WIRE_NUMBER(offset);
        MVT_WIRE_NUMBER(duringSession);MVT_WIRE_NUMBER(freeHeap);
        MVT_WIRE_NUMBER(largestBlock);MVT_WIRE_NUMBER(minimumHeap);
#undef MVT_WIRE_NUMBER
        return w.finish();
    }
    cJSON *p = cJSON_CreateObject();
    if (!p) return nullptr;
    mvtWssDiagnosticField(p, "stage", cJSON_CreateString(d.stage));
#define MVT_DIAG_NUMBER(name) mvtWssDiagnosticField(p, #name, cJSON_CreateNumber(d.name))
    MVT_DIAG_NUMBER(bootId); MVT_DIAG_NUMBER(sequence); MVT_DIAG_NUMBER(uptimeMs);
    MVT_DIAG_NUMBER(ret); MVT_DIAG_NUMBER(error); MVT_DIAG_NUMBER(socketError);
    MVT_DIAG_NUMBER(tlsError); MVT_DIAG_NUMBER(tlsCode); MVT_DIAG_NUMBER(tlsFlags);
    MVT_DIAG_NUMBER(opcode); MVT_DIAG_NUMBER(payload); MVT_DIAG_NUMBER(offset);
    MVT_DIAG_NUMBER(duringSession);
    MVT_DIAG_NUMBER(freeHeap); MVT_DIAG_NUMBER(largestBlock); MVT_DIAG_NUMBER(minimumHeap);
#undef MVT_DIAG_NUMBER
    return p;
}
static inline cJSON *mvtWssBootJson(bool wire = false) {
    MvtWssBootDiagnostic d;
    if (!mvt_wss_boot_diagnostic(&d)) return nullptr;
    if(wire){
        MvtDiagnosticWire w;
#define MVT_WIRE_BOOT(name) w.number(#name,d.name)
        MVT_WIRE_BOOT(bootId);MVT_WIRE_BOOT(resetReason);MVT_WIRE_BOOT(wakeCause);
        MVT_WIRE_BOOT(bootCount);MVT_WIRE_BOOT(previousSleepUptimeMs);
        MVT_WIRE_BOOT(previousSleepDurationMs);MVT_WIRE_BOOT(sleepIntent);
#undef MVT_WIRE_BOOT
        return w.finish();
    }
    cJSON *p = cJSON_CreateObject(); if (!p) return nullptr;
#define MVT_BOOT_NUMBER(name) mvtWssDiagnosticField(p, #name, cJSON_CreateNumber(d.name))
    MVT_BOOT_NUMBER(bootId); MVT_BOOT_NUMBER(resetReason); MVT_BOOT_NUMBER(wakeCause);
    MVT_BOOT_NUMBER(bootCount); MVT_BOOT_NUMBER(previousSleepUptimeMs);
    MVT_BOOT_NUMBER(previousSleepDurationMs); MVT_BOOT_NUMBER(sleepIntent);
#undef MVT_BOOT_NUMBER
    return p;
}
