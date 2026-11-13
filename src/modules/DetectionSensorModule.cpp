#include "DetectionSensorModule.h"
#include "Default.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "configuration.h"
#include "main.h"
#include <Throttle.h>
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
#include <SHA256.h>
#include "RTC.h"
#endif
DetectionSensorModule *detectionSensorModule;

#define GPIO_POLLING_INTERVAL 100
#define DELAYED_INTERVAL 1000

typedef enum {
    DetectionSensorVerdictDetected,
    DetectionSensorVerdictSendState,
    DetectionSensorVerdictNoop,
} DetectionSensorTriggerVerdict;

typedef DetectionSensorTriggerVerdict (*DetectionSensorTriggerHandler)(bool prev, bool current);

static DetectionSensorTriggerVerdict detection_trigger_logic_level(bool prev, bool current)
{
    return current ? DetectionSensorVerdictDetected : DetectionSensorVerdictNoop;
}

static DetectionSensorTriggerVerdict detection_trigger_single_edge(bool prev, bool current)
{
    return (!prev && current) ? DetectionSensorVerdictDetected : DetectionSensorVerdictNoop;
}

static DetectionSensorTriggerVerdict detection_trigger_either_edge(bool prev, bool current)
{
    if (prev == current) {
        return DetectionSensorVerdictNoop;
    }
    return current ? DetectionSensorVerdictDetected : DetectionSensorVerdictSendState;
}

const static DetectionSensorTriggerHandler handlers[_meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_MAX + 1] = {
    [meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_LOGIC_LOW] = detection_trigger_logic_level,
    [meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_LOGIC_HIGH] = detection_trigger_logic_level,
    [meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_FALLING_EDGE] = detection_trigger_single_edge,
    [meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_RISING_EDGE] = detection_trigger_single_edge,
    [meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_EITHER_EDGE_ACTIVE_LOW] = detection_trigger_either_edge,
    [meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_EITHER_EDGE_ACTIVE_HIGH] = detection_trigger_either_edge,
};

int32_t DetectionSensorModule::runOnce()
{
    /*
        Uncomment the preferences below if you want to use the module
        without having to configure it from the PythonAPI or WebUI.
    */
    // moduleConfig.detection_sensor.enabled = true;
    // moduleConfig.detection_sensor.monitor_pin = 10; // WisBlock PIR IO6
    // moduleConfig.detection_sensor.monitor_pin = 21; // WisBlock RAK12013 Radar IO6
    // moduleConfig.detection_sensor.minimum_broadcast_secs = 30;
    // moduleConfig.detection_sensor.state_broadcast_secs = 120;
    // moduleConfig.detection_sensor.detection_trigger_type =
    // meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_LOGIC_HIGH;
    // strcpy(moduleConfig.detection_sensor.name, "Motion");

    if (moduleConfig.detection_sensor.enabled == false)
        return disable();

    if (firstTime) {

#ifdef DETECTION_SENSOR_EN
        pinMode(DETECTION_SENSOR_EN, OUTPUT);
        digitalWrite(DETECTION_SENSOR_EN, HIGH);
#endif

        // This is the first time the OSThread library has called this function, so do some setup
        firstTime = false;
        if (moduleConfig.detection_sensor.monitor_pin > 0) {
            pinMode(moduleConfig.detection_sensor.monitor_pin, moduleConfig.detection_sensor.use_pullup ? INPUT_PULLUP : INPUT);
        } else {
            LOG_WARN("Detection Sensor Module: Set to enabled but no monitor pin is set. Disable module");
            return disable();
        }
        LOG_INFO("Detection Sensor Module: init");

        return setStartDelay();
    }

    #ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    sendPending();
    #endif
    // LOG_DEBUG("Detection Sensor Module: Current pin state: %i", digitalRead(moduleConfig.detection_sensor.monitor_pin));

    if (!Throttle::isWithinTimespanMs(lastSentToMesh,
                                      Default::getConfiguredOrDefaultMs(moduleConfig.detection_sensor.minimum_broadcast_secs))) {
        bool isDetected = hasDetectionEvent();
        DetectionSensorTriggerVerdict verdict =
            handlers[moduleConfig.detection_sensor.detection_trigger_type](wasDetected, isDetected);
        wasDetected = isDetected;
        switch (verdict) {
        case DetectionSensorVerdictDetected:
            sendDetectionMessage();
            return DELAYED_INTERVAL;
        case DetectionSensorVerdictSendState:
            sendCurrentStateMessage(isDetected);
            return DELAYED_INTERVAL;
        case DetectionSensorVerdictNoop:
            break;
        }
    }
    // Even if we haven't detected an event, broadcast our current state to the mesh on the scheduled interval as a sort
    // of heartbeat. We only do this if the minimum broadcast interval is greater than zero, otherwise we'll only broadcast state
    // change detections.
    if (moduleConfig.detection_sensor.state_broadcast_secs > 0 &&
        !Throttle::isWithinTimespanMs(lastSentToMesh,
                                      Default::getConfiguredOrDefaultMs(moduleConfig.detection_sensor.state_broadcast_secs,
                                                                        default_telemetry_broadcast_interval_secs))) {
        sendCurrentStateMessage(hasDetectionEvent());
        return DELAYED_INTERVAL;
    }
    return GPIO_POLLING_INTERVAL;
}

void DetectionSensorModule::sendDetectionMessage()
{
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    queueEvent(true, true);
    return;
#endif
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    const auto &channel = channels.getByName(Channels::gpioChannel);
    // getByName falls back to the primary channel when the name is missing.
    if (channel.role == meshtastic_Channel_Role_DISABLED ||
        strcasecmp(channel.settings.name, Channels::gpioChannel) != 0) {
        LOG_ERROR("Detection Sensor: configure an enabled gpio channel before sending");
        return;
    }
#endif
    LOG_DEBUG("Detected event observed. Send message");
    char *message = new char[40];
    sprintf(message, "%s detected", moduleConfig.detection_sensor.name);
    meshtastic_MeshPacket *p = allocDataPacket();
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    p->channel = channel.index;
#endif
    p->want_ack = false;
    p->decoded.payload.size = strlen(message);
    memcpy(p->decoded.payload.bytes, message, p->decoded.payload.size);
    if (moduleConfig.detection_sensor.send_bell && p->decoded.payload.size < meshtastic_Constants_DATA_PAYLOAD_LEN) {
        p->decoded.payload.bytes[p->decoded.payload.size] = 7;        // Bell character
        p->decoded.payload.bytes[p->decoded.payload.size + 1] = '\0'; // Bell character
        p->decoded.payload.size++;
    }
    lastSentToMesh = millis();
    if (
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
        true
#else
        !channels.isDefaultChannel(0)
#endif
    ) {
        LOG_INFO("Send message id=%d, dest=%x, msg=%.*s", p->id, p->to, p->decoded.payload.size, p->decoded.payload.bytes);
        service->sendToMesh(p);
    } else
        LOG_ERROR("Message not allow on Public channel");
    delete[] message;
}

void DetectionSensorModule::sendCurrentStateMessage(bool state)
{
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    queueEvent(state, false);
    return;
#endif
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    const auto &channel = channels.getByName(Channels::gpioChannel);
    if (channel.role == meshtastic_Channel_Role_DISABLED ||
        strcasecmp(channel.settings.name, Channels::gpioChannel) != 0) {
        LOG_ERROR("Detection Sensor: configure an enabled gpio channel before sending");
        return;
    }
#endif
    char *message = new char[40];
    sprintf(message, "%s state: %i", moduleConfig.detection_sensor.name, state);
    meshtastic_MeshPacket *p = allocDataPacket();
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    p->channel = channel.index;
#endif
    p->want_ack = false;
    p->decoded.payload.size = strlen(message);
    memcpy(p->decoded.payload.bytes, message, p->decoded.payload.size);
    lastSentToMesh = millis();
    if (
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
        true
#else
        !channels.isDefaultChannel(0)
#endif
    ) {
        LOG_INFO("Send message id=%d, dest=%x, msg=%.*s", p->id, p->to, p->decoded.payload.size, p->decoded.payload.bytes);
        service->sendToMesh(p);
    } else
        LOG_ERROR("Message not allow on Public channel");
    delete[] message;
}

bool DetectionSensorModule::hasDetectionEvent()
{
    bool currentState = digitalRead(moduleConfig.detection_sensor.monitor_pin);
    // LOG_DEBUG("Detection Sensor Module: Current state: %i", currentState);
    return (moduleConfig.detection_sensor.detection_trigger_type & 1) ? currentState : !currentState;
}

#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
// ACK authenticates possession of the gpio PSK, not a unique human identity.
static void deliveryMac(const uint8_t *key, size_t keySize, const uint8_t *body, size_t size, uint8_t *out)
{
    SHA256 hash; hash.resetHMAC(key,keySize); hash.update(body,size); hash.finalizeHMAC(key,keySize,out,16);
}
bool DetectionSensorModule::prepareDelivery()
{
    const auto &ch=channels.getByName(Channels::gpioChannel);
    const auto &key=ch.settings.psk;
    if(ch.role==meshtastic_Channel_Role_DISABLED || strcasecmp(ch.settings.name,"gpio") || (key.size!=16 && key.size!=32)) {
        delivery.clear(); deliveryKeySize=0; return false;
    }
    if(deliveryKeySize!=key.size || memcmp(deliveryKey,key.bytes,key.size)) {
        delivery.clear(); deliveryKeySize=key.size; memcpy(deliveryKey,key.bytes,key.size);
        SensorDelivery::put32(stream,uint32_t(random())); SensorDelivery::put32(stream+4,uint32_t(random())); sequence=0;
    }
    return true;
}
void DetectionSensorModule::queueEvent(bool active, bool detection)
{
    lastSentToMesh=millis();
    if(!prepareDelivery())return;
    auto &item=delivery.allocate(millis()); auto *b=item.bytes;
    memcpy(b,"MDS2",4); b[4]=1; SensorDelivery::put32(b+5,nodeDB->getNodeNum()); memcpy(b+9,stream,8);
    SensorDelivery::put32(b+17,++sequence);
    SensorDelivery::put32(b+21,getValidTime(RTCQualityFromNet));
    SensorDelivery::put32(b+25,moduleConfig.detection_sensor.state_broadcast_secs);
    b[29]=delivery.dropped; b[30]=delivery.dropped>>8; b[31]=moduleConfig.detection_sensor.monitor_pin;
    b[32]=active; b[33]=detection;
    const char *name=moduleConfig.detection_sensor.name[0]?moduleConfig.detection_sensor.name:"Sensor";
    b[34]=strnlen(name,20); memcpy(b+35,name,b[34]); item.size=35+b[34]+16;
    deliveryMac(deliveryKey,deliveryKeySize,b,item.size-16,b+item.size-16);
    sendPending();
}
void DetectionSensorModule::sendPending()
{
    if(!prepareDelivery())return;
    uint32_t now=millis();
    if(lastDelivery && uint32_t(now-lastDelivery)<15000)return;
    auto *item=delivery.due(now); if(!item)return;
    auto *p=allocDataPacket(); p->channel=channels.getByName(Channels::gpioChannel).index;
    p->want_ack=false; p->decoded.payload.size=item->size; memcpy(p->decoded.payload.bytes,item->bytes,item->size);
    item->last=now; ++item->attempts; lastDelivery=now; service->sendToMesh(p);
}
ProcessMessage DetectionSensorModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if(!moduleConfig.detection_sensor.enabled || !prepareDelivery())return ProcessMessage::CONTINUE;
    const auto &data=mp.decoded.payload; const auto *b=data.bytes;
    if(mp.channel!=channels.getByName(Channels::gpioChannel).index || mp.to!=nodeDB->getNodeNum()
        || data.size!=57 || memcmp(b,"MDS2",4) || b[4]!=2 || SensorDelivery::get32(b+5)!=mp.from
        || SensorDelivery::get32(b+9)!=nodeDB->getNodeNum())return ProcessMessage::CONTINUE;
    uint8_t tag[16]; deliveryMac(deliveryKey,deliveryKeySize,b,41,tag);
    uint8_t different=0; for(size_t i=0;i<16;i++)different|=tag[i]^b[41+i];
    if(!different && delivery.acknowledge(b))LOG_INFO("Detection Sensor: portal receipt confirmed");
    return ProcessMessage::STOP;
}
#endif
