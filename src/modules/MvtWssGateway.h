#pragma once
#if HAS_MVT_WSS_GATEWAY
#include "MeshModule.h"
#include "concurrency/OSThread.h"
#include "esp_websocket_client.h"
#include <WiFi.h>
#include <cJSON.h>
#include <deque>
#include <map>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Websocket callbacks only queue input; mesh/protocol state belongs to runOnce.
class MvtWssGateway : public MeshModule, private concurrency::OSThread {
public:
    MvtWssGateway();
    ~MvtWssGateway() override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &) override;
    bool wantPacket(const meshtastic_MeshPacket *) override;
    static void handleWsEvent(void *, esp_event_base_t, int32_t, void *);
    bool isWsConnected() const { return authenticated && socketConnected; }
    const char *displayStatus() const;
    uint8_t effectiveHopLimit() const;
    const char *radioPresetName() const;
protected:
    int32_t runOnce() override;
private:
    struct Pending {
        Pending() = default;
        Pending(uint32_t packet, uint32_t node, uint32_t tick, const std::string &type,
                const std::string &key, bool mv) : id(packet), target(node), started(tick), kind(type), nodeKey(key), isVirtual(mv) {}
        uint32_t id, target, started;
        std::string kind, nodeKey;
        bool isVirtual;
        bool positionReceived = false, telemetryReceived = false, nodeInfoReceived = false;
    };
    struct Poll {
        Pending command;
        uint32_t ids[3] = {}, nextAt = 0, spacing = 3000;
        uint8_t count = 0, sent = 0, replies = 0;
        bool needsInfo = false;
    };
    std::deque<Poll> polls;
    void advancePolls();
    esp_websocket_client_handle_t wsClient = nullptr;
    QueueHandle_t incoming = nullptr;
    volatile bool socketConnected = false, inputFault = false, peerRejected = false;
    bool authenticated = false, authSent = false, authBlocked = false;
    bool snapshotPending = false;
    uint8_t portalHopLimit = 0; // Zero means use the radio configuration.
    const char *displayError = nullptr;
    uint32_t lastHeartbeatMs = 0, lastConnectAttemptMs = 0, authStartedMs = 0;
    uint64_t serverTimeMs = 0;
    uint32_t serverTimeTick = 0;
    std::string frame, configuredRoom, configuredKey, configuredAddress;
    size_t frameExpected = 0;
    std::deque<std::string> outgoing;
    std::deque<std::string> snapshot;
    std::deque<Pending> pending;
    std::map<std::string, std::string> nodes, waypoints;
    std::deque<uint64_t> seenPackets;
    std::deque<uint32_t> completedCommands;
    void connectWs();
    void disconnectWs();
    void receiveFrame(const esp_websocket_event_data_t &);
    bool sendDocument(cJSON *);
    void publish(cJSON *);
    void sendAuthGateway();
    void sendHeartbeat();
    void sendSnapshot();
    cJSON *gatewayInfo();
    cJSON *nodeDocument(uint32_t, bool = false);
    void publishNode(cJSON *);
    void publishWaypoint(cJSON *);
    void handleIncomingCommand(const char *);
    void status(uint32_t, const char *);
    void finishDiagnostic(const Pending &, bool, const meshtastic_MeshPacket * = nullptr,
                          const meshtastic_Telemetry * = nullptr);
    meshtastic_MeshPacket *packet(uint32_t, uint32_t = 0);
    bool transmit(meshtastic_MeshPacket *);
    uint64_t nowMs() const;
};
extern MvtWssGateway *mvtWssGateway;
#endif
