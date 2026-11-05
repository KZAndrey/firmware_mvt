#pragma once
#include <string>
#include <cJSON.h>
#define MVT_OTA_GENERATION 1
extern bool testOtaWorkerRunning;
extern bool testOtaActive;
extern const char *testOtaStartError;
extern unsigned testOtaStartCalls;
struct MvtOtaUpdater {
 void begin(){}
 void onDisconnect(){}
 bool workerRunning()const{return testOtaWorkerRunning;}
 bool supported()const{return false;}
 bool start(const cJSON *,const std::string &,uint32_t){++testOtaStartCalls;return false;}
 const char *startError()const{return testOtaStartError;}
 bool chunk(const cJSON *){return false;}
 void tick(bool){}
 bool changed(){return false;}
 bool active()const{return testOtaActive;}
 bool hasStatus()const{return false;}
 cJSON *status(uint32_t)const{return cJSON_CreateObject();}
};
static MvtOtaUpdater mvtOtaUpdater;
struct TestEsp {unsigned getFreeHeap(){return 100000;}unsigned getMaxAllocHeap(){return 100000;}};
static TestEsp ESP;
