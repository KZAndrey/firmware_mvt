#pragma once
#include "SinglePortModule.h"
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
#include "SensorDelivery.h"
#endif

class DetectionSensorModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    DetectionSensorModule() : SinglePortModule("detection", meshtastic_PortNum_DETECTION_SENSOR_APP), OSThread("DetectionSensor")
    {
    }

  protected:
    virtual int32_t runOnce() override;
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
#endif

  private:
    #ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
    SensorDelivery::Queue delivery;
    uint8_t deliveryKey[32] = {}, deliveryKeySize = 0, stream[8] = {};
    uint32_t sequence = 0, lastDelivery = 0;
    bool prepareDelivery();
    void queueEvent(bool active, bool detection);
    void sendPending();
#endif
    bool firstTime = true;
    uint32_t lastSentToMesh = 0;
    bool wasDetected = false;
    void sendDetectionMessage();
    void sendCurrentStateMessage(bool state);
    bool hasDetectionEvent();
};

extern DetectionSensorModule *detectionSensorModule;