#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY
#include "MvtOtaUpdater.h"
#include "MvtOtaPublicKey.h"
#include "NodeDB.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <freertos/timers.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <cmath>
#include <cstring>
#include <algorithm>

// Defer Arduino's automatic early approval until application/WSS health checks.
extern "C" bool verifyRollbackLater(){return true;}
MvtOtaUpdater mvtOtaUpdater;
static TimerHandle_t trialTimer=nullptr;
static const char imageMarker[]="MVTOTA1:heltec-v3:" xstr(MVT_OTA_GENERATION) ":" xstr(APP_VERSION);
namespace {
const char *text(const cJSON *j,const char *key){auto *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:"";}
double num(const cJSON *j,const char *key){auto *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsNumber(v)?v->valuedouble:-1;}
bool token(const std::string &s,size_t max){if(s.empty()||s.size()>max)return false;for(char c:s)if(!isalnum(uint8_t(c))&&c!='-'&&c!='_')return false;return true;}
bool integer(double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high&&floor(n)==n;}
const char *names[]={"idle","downloading","verifying","installing","restarting","checking","updated","rolled_back","failed"};
}
bool MvtOtaUpdater::supported() const {
#if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    return false;
#else
    auto *p=esp_ota_get_next_update_partition(nullptr);
    return p&&p!=esp_ota_get_running_partition()&&p->size>=0x1c0000;
#endif
}
void MvtOtaUpdater::begin(){
    LOG_INFO("MVT OTA image %s",imageMarker);
    Preferences prefs;if(prefs.begin("mvt-ota",true)){
        jobId=prefs.getString("job","").c_str();releaseId=prefs.getString("release","").c_str();
        roomId=prefs.getString("room","").c_str();nodeId=prefs.getUInt("node",0);
        generation=prefs.getUInt("gen",0);prefs.end();
    }
    esp_ota_img_states_t imageState;
    trial=esp_ota_get_state_partition(esp_ota_get_running_partition(),&imageState)==ESP_OK&&imageState==ESP_OTA_IMG_PENDING_VERIFY;
    if(trial){
        state=Checking;
        // If health never completes (including a wedged application thread),
        // reset while PENDING_VERIFY so the bootloader rejects this candidate.
        trialTimer=xTimerCreate("ota-trial",pdMS_TO_TICKS(120000),pdFALSE,nullptr,[](TimerHandle_t){esp_restart();});
        if(!trialTimer||xTimerStart(trialTimer,0)!=pdPASS)esp_ota_mark_app_invalid_rollback_and_reboot();
    }else if(!jobId.empty()){state=generation==MVT_OTA_GENERATION?Updated:RolledBack;percent=100;}
}
bool MvtOtaUpdater::active() const {int s=state;return s>=Downloading&&s<=Checking;}
const char *MvtOtaUpdater::stage() const {return names[state.load()];}
void MvtOtaUpdater::fail(const char *why){error=why;state=Failed;}
bool MvtOtaUpdater::changed(){
    int s=state.load(),p=percent.load();uint32_t offset=downloadOffset.load(),now=millis();
    if(s==lastState&&p==lastPercent&&offset==lastOffset&&(!active()||uint32_t(now-lastStatusMs)<5000))return false;
    lastState=s;lastPercent=p;lastOffset=offset;lastStatusMs=now;return true;
}
cJSON *MvtOtaUpdater::status(uint32_t id) const {
    auto *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"type","FIRMWARE_UPDATE_STATE");
    cJSON_AddStringToObject(j,"jobId",jobId.c_str());cJSON_AddNumberToObject(j,"nodeId",id);
    cJSON_AddStringToObject(j,"state",stage());cJSON_AddNumberToObject(j,"percent",percent.load());
    if(state.load()==Downloading)cJSON_AddNumberToObject(j,"downloadOffset",downloadOffset.load());
    // error is written before the release-store to Failed; readers inspect it only then.
    if(state.load()==Failed)cJSON_AddStringToObject(j,"error",error.c_str());
    return j;
}
bool MvtOtaUpdater::start(const cJSON *command,const std::string &room,uint32_t id){
    if(active()||!supported()||std::string(text(command,"transfer"))!="wss-chunks-v2")return false;
    if(std::string(text(command,"roomId"))!=room||num(command,"nodeId")!=id)return false;
    const auto *m=cJSON_GetObjectItemCaseSensitive(command,"manifest");
    const std::string release=text(m,"releaseId"),v=text(m,"version"),h=text(m,"sha256"),imagePath=text(m,"imagePath"),job=text(command,"jobId");
    if(num(m,"schema")!=1||std::string(text(m,"model"))!="heltec-v3"||std::string(text(m,"chip"))!="esp32s3"||
       !token(release,64)||!token(job,64)||v.size()>40||v.find("2.7.15.")!=0||
       !integer(num(m,"generation"),MVT_OTA_GENERATION+1,2147483647)||!integer(num(m,"size"),1024,0x330000)||
       h.size()!=64||imagePath!="/firmware/heltec-v3/"+release+".bin")return false;
    for(char c:v)if(!isalnum(uint8_t(c))&&c!='.'&&c!='-')return false;
    for(char c:h)if(!isdigit(uint8_t(c))&&(c<'a'||c>'f'))return false;
    // Require enough power before touching the inactive application slot.
    // USB/external power is checked by the gateway before calling start().
    jobId=job;roomId=room;nodeId=id;releaseId=release;version=v;hash=h;
    generation=uint32_t(num(m,"generation"));bytes=uint32_t(num(m,"size"));signature=text(m,"signature");
    if(signature.size()!=344)return false;
    if(!chunks)chunks=xQueueCreate(1,sizeof(Chunk));
    if(!chunks)return false;
    xQueueReset(chunks);downloadOffset=0;offeredOffset=UINT32_MAX;
    error.clear();percent=0;state=Verifying;
    TaskHandle_t handle=nullptr;
    if(xTaskCreate([](void *ctx){static_cast<MvtOtaUpdater *>(ctx)->run();vTaskDelete(nullptr);},"mvt-ota",12288,this,1,&handle)!=pdPASS){fail("TASK_MEMORY");return false;}
    return true;
}
bool MvtOtaUpdater::chunk(const cJSON *command){
    if(state.load()!=Downloading||std::string(text(command,"jobId"))!=jobId||
       std::string(text(command,"roomId"))!=roomId||num(command,"nodeId")!=nodeId)return false;
    double offset=num(command,"offset");
    if(!integer(offset,0,bytes-1)||offset!=downloadOffset.load()||offeredOffset.load()==uint32_t(offset))return false;
    const char *data=text(command,"data");size_t encoded=strlen(data);
    if(!encoded||encoded>2732)return false;
    Chunk incoming={};incoming.offset=uint32_t(offset);
    if(mbedtls_base64_decode(incoming.data,sizeof(incoming.data),&incoming.length,(const uint8_t *)data,encoded)!=0||
       incoming.length!=std::min(size_t(2048),size_t(bytes-incoming.offset)))return false;
    // One bounded chunk in flight; duplicates never replace the next offset.
    offeredOffset=incoming.offset;
    if(xQueueSend(chunks,&incoming,0)!=pdPASS){offeredOffset=UINT32_MAX;return false;}
    return true;
}
void MvtOtaUpdater::run(){
    std::string canonical="MVT-OTA-1\nheltec-v3\nesp32s3\n"+releaseId+"\n"+version+"\n"+std::to_string(generation)+"\n"+std::to_string(bytes)+"\n"+hash+"\n/firmware/heltec-v3/"+releaseId+".bin\n";
    uint8_t digest[32],sig[256];size_t sigLen=0;mbedtls_sha256_ret((const uint8_t *)canonical.data(),canonical.size(),digest,0);
    mbedtls_pk_context pk;mbedtls_pk_init(&pk);
    bool verified=mbedtls_pk_parse_public_key(&pk,(const uint8_t *)MVT_OTA_PUBLIC_KEY,sizeof(MVT_OTA_PUBLIC_KEY))==0&&
      mbedtls_base64_decode(sig,sizeof(sig),&sigLen,(const uint8_t *)signature.data(),signature.size())==0&&sigLen==256&&
      mbedtls_pk_verify(&pk,MBEDTLS_MD_SHA256,digest,32,sig,sigLen)==0;
    mbedtls_pk_free(&pk);if(!verified){fail("BAD_SIGNATURE");return;}
    const esp_partition_t *target=esp_ota_get_next_update_partition(nullptr);
    if(!target||bytes>target->size){fail("PARTITION_SIZE");return;}
    esp_ota_handle_t ota=0;bool opened=false;const char *why=nullptr;
    do{
        if(esp_ota_begin(target,bytes,&ota)!=ESP_OK){why="OTA_BEGIN";break;}opened=true;state=Downloading;
        mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts_ret(&sha,0);
        Chunk incoming={};uint32_t done=0,started=millis();
        std::string marker="MVTOTA1:heltec-v3:"+std::to_string(generation)+":"+version;marker.push_back('\0');
        std::string tail;bool markerFound=false;
        while(done<bytes){
            // Progress may continue on a slow link; the idle receive timeout
            // below detects a stalled transfer independently of this hard cap.
            if(uint32_t(millis()-started)>1800000){why="DOWNLOAD_TIMEOUT";break;}
            if(xQueueReceive(chunks,&incoming,pdMS_TO_TICKS(60000))!=pdPASS){why="WSS_DOWNLOAD_TIMEOUT";break;}
            if(incoming.offset!=done){why="CHUNK_OFFSET";break;}
            size_t n=incoming.length;
            mbedtls_sha256_update_ret(&sha,incoming.data,n);
            std::string window=tail+std::string((char *)incoming.data,n);if(window.find(marker)!=std::string::npos)markerFound=true;
            tail=window.substr(window.size()>80?window.size()-80:0);
            if(esp_ota_write(ota,incoming.data,n)!=ESP_OK){why="FLASH_WRITE";break;}
            done+=n;percent=done*100/bytes;downloadOffset=done;vTaskDelay(1);
        }
        mbedtls_sha256_finish_ret(&sha,digest);mbedtls_sha256_free(&sha);if(why)break;
        state=Verifying;char hex[65];for(int k=0;k<32;k++)sprintf(hex+k*2,"%02x",digest[k]);
        if(hash!=hex){why="IMAGE_HASH";break;}
        if(!markerFound){why="IMAGE_GENERATION";break;}
        esp_err_t ended=esp_ota_end(ota);opened=false;
        if(ended!=ESP_OK){why="IMAGE_INVALID";break;}
        esp_app_desc_t desc={};
        if(esp_ota_get_partition_description(target,&desc)!=ESP_OK){why="IMAGE_DESCRIPTION";break;}
        if(roomId!=moduleConfig.mqtt.username||nodeId!=nodeDB->getNodeNum()){why="DEVICE_ROOM_CHANGED";break;}
        state=Installing;
        Preferences prefs;if(!prefs.begin("mvt-ota",false)){why="STATE_STORAGE";break;}
        bool saved=prefs.putString("job",jobId.c_str())&&prefs.putString("release",releaseId.c_str())&&prefs.putString("room",roomId.c_str())&&prefs.putUInt("node",nodeId)&&prefs.putUInt("gen",generation);prefs.end();
        if(!saved){why="STATE_STORAGE";break;}
        if(esp_ota_set_boot_partition(target)!=ESP_OK){why="BOOT_PARTITION";break;}
        state=Restarting;
    }while(false);
    if(opened)esp_ota_abort(ota);
    if(why){fail(why);return;}
    vTaskDelay(pdMS_TO_TICKS(2000));esp_restart();
}
void MvtOtaUpdater::tick(bool healthy){
    if(!trial)return;
    if(generation!=MVT_OTA_GENERATION||roomId!=moduleConfig.mqtt.username||nodeId!=nodeDB->getNodeNum())healthy=false;
    if(!healthy){healthySince=0;return;}
    if(!healthySince)healthySince=millis();
    if(uint32_t(millis()-healthySince)<30000)return;
    if(esp_ota_mark_app_valid_cancel_rollback()!=ESP_OK)return;
    if(trialTimer){xTimerStop(trialTimer,0);xTimerDelete(trialTimer,0);trialTimer=nullptr;}
    trial=false;percent=100;state=Updated;
}
#endif
