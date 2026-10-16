#pragma once
#include <string>
#include <cJSON.h>
#define MVT_OTA_GENERATION 1
struct MvtOtaUpdater {
 void begin(){}
 bool supported()const{return false;}
 bool start(const cJSON *,const std::string &,uint32_t){return false;}
 bool chunk(const cJSON *){return false;}
 void tick(bool){}
 bool changed(){return false;}
 bool active()const{return false;}
 bool hasStatus()const{return false;}
 cJSON *status(uint32_t)const{return cJSON_CreateObject();}
};
static MvtOtaUpdater mvtOtaUpdater;
struct TestEsp {unsigned getFreeHeap(){return 100000;}};
static TestEsp ESP;
