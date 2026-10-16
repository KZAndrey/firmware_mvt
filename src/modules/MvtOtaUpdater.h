#pragma once
#if HAS_MVT_WSS_GATEWAY
#include <atomic>
#include <string>
#include <cJSON.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "MvtOtaGeneration.h"
#ifndef MVT_OTA_GENERATION
#define MVT_OTA_GENERATION 1
#endif
class MvtOtaUpdater {
public:
    void begin();
    bool start(const cJSON *,const std::string &,uint32_t);
    bool chunk(const cJSON *);
    void tick(bool healthy);
    cJSON *status(uint32_t) const;
    bool active() const;
    bool hasStatus() const{return !jobId.empty();}
    const char *stage() const;
    unsigned progress() const {return percent.load();}
    bool supported() const;
    bool changed();
private:
    enum State {Idle,Downloading,Verifying,Installing,Restarting,Checking,Updated,RolledBack,Failed};
    std::atomic<int> state{Idle};
    std::atomic<unsigned> percent{0};
    int lastState=-1,lastPercent=-1;
    uint32_t lastOffset=UINT32_MAX,lastStatusMs=0;
    std::atomic<uint32_t> downloadOffset{0},offeredOffset{UINT32_MAX};
    struct Chunk {uint32_t offset;size_t length;uint8_t data[1024];};
    QueueHandle_t chunks=nullptr;
    std::string jobId,roomId,releaseId,version,hash,signature,error;
    uint32_t nodeId=0,generation=0,bytes=0,healthySince=0;
    bool trial=false;
    void run();
    void fail(const char *);
};
extern MvtOtaUpdater mvtOtaUpdater;
#endif
