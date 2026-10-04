#pragma once

#if HAS_MVT_WSS_GATEWAY

#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "esp_websocket_client.h"
#include <WiFi.h>

/**
 * @brief MVT WSS Gateway Module for Movetain Ecosystem
 * Bridges field LoRa mesh (Meshtastic) directly with Movetain Cloud (WSS) without requiring an intermediate smartphone.
 * Consumes zero MQTT overhead, saving ~150KB RAM.
 */
class MvtWssGateway : public MeshModule, private concurrency::OSThread
{
public:
    MvtWssGateway();
    virtual ~MvtWssGateway();

    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;

    static void handleWsEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

    void onWsConnected();
    void onWsDisconnected();
    void onWsData(const char *data, int len);

    bool sendJson(const char *jsonStr);
    bool isWsConnected() const { return wsConnected; }

protected:
    virtual int32_t runOnce() override;

private:
    esp_websocket_client_handle_t wsClient = nullptr;
    bool wsConnected = false;
    uint32_t lastHeartbeatMs = 0;
    uint32_t lastConnectAttemptMs = 0;

    void connectWs();
    void disconnectWs();
    void sendAuthGateway();
    void sendHeartbeat();
    void handleIncomingCommand(const char *jsonStr);
};

extern MvtWssGateway *mvtWssGateway;

#endif // HAS_MVT_WSS_GATEWAY
