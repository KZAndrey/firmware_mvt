#pragma once
#if HAS_MVT_WSS_GATEWAY
#include "MeshModule.h"
#include "mesh/generated/meshtastic/admin.pb.h"
#include "concurrency/OSThread.h"
#include "platform/esp32/MvtWssTransport.h"
#include "MvtWssNetworkRecovery.h"
#include "MvtWssRxBuffer.h"
#include <atomic>
#include <WiFi.h>
#include <cJSON.h>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Websocket callbacks only queue input; mesh/protocol state belongs to runOnce.
class MvtWssGateway : public MeshModule, private concurrency::OSThread {
public:
    MvtWssGateway();
    ~MvtWssGateway() override;
    void acceptVerifiedContact(const meshtastic_SharedContact &contact);
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
#if MVT_WSS_STRESS_TEST
    void stressTick();
    void stressSample(const char *phase);
    uint32_t stressCycle=0, stressBadTargetUntil=0, stressIdleAt=0;
    bool stressWifiHeld=false, stressIdlePending=false, stressHalted=false;
    bool stressRadioPending=false;
    bool stressRejoinPending=false;
#endif
    struct Pending {
        Pending() = default;
        Pending(uint32_t packet, uint32_t node, uint32_t tick, const std::string &type,
                const std::string &key, bool mv) : id(packet), target(node), started(tick), kind(type), nodeKey(key), isVirtual(mv) {}
        uint32_t id, target, started, timeout = 0;
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
    struct AirPoll {
        uint32_t target=0, started=0, nextAt=0, spacing=3000, timeout=0, ids[2]={};
        uint8_t sent=0, completed=0;
        bool environmentReceived=false, airReceived=false;
        std::string nodeKey;
        std::map<std::string,double> values;
    };
    std::set<uint32_t> pkiConflicts;
    std::deque<AirPoll> airPolls;
    uint32_t diagnosticStep(uint32_t) const;
    uint32_t diagnosticReply(uint32_t) const;
    uint32_t diagnosticTimeout(uint32_t = 0, unsigned = 1, bool = false) const;
    uint32_t nextDiagnosticAt = 0, lastDiagnosticState = 0, lastDiagnosticDispatch = 0, diagnosticGap = 0;
    bool diagnosticSlot() const;
    void advanceSingles();
    void diagnosticState(uint32_t, const std::string &, const char *, uint32_t, bool, uint32_t = 45000, const char * = "");
    void sendDiagnosticStates();
    void commandThrottled(uint32_t, const std::string &, const std::string &);
    void retireReportedPolls(uint32_t);
    void advanceAirPolls();
    void airResult(const AirPoll &, bool, const char * = nullptr);
    std::deque<Poll> polls;
    void advancePolls();
    MvtWssTransport transport{handleWsEvent, this};
    MvtWssNetworkRecovery networkRecovery;
    bool retiring = false, retirementNeedsDelay = false;
    uint32_t configurationEpoch = 0, retryDelayMs = 15000;
    QueueHandle_t incoming = nullptr;
    std::atomic<bool> socketConnected{false}, inputFault{false}, peerRejected{false};
    std::atomic<bool> connectionEnded{false};
    bool connecting = false;
    uint32_t connectStartedMs = 0;
    std::atomic<uint32_t> lastServerAckMs{0}, lastHeartbeatMs{0};
    bool authenticated = false, authSent = false, authBlocked = false;
    bool snapshotPending = false;
    const char *displayError = nullptr;
    int lastWifiStatus = -1;
    uint32_t lastConnectAttemptMs = 0, authStartedMs = 0;
    uint64_t serverTimeMs = 0;
    uint32_t serverTimeTick = 0;
    MvtWssRxBuffer frame;
    std::string configuredRoom, configuredKey, configuredAddress;
    size_t frameExpected = 0;
    std::atomic<size_t> incomingBytes{0};
    std::deque<std::string> outgoing;
    void bufferEvent(std::string);
    std::deque<std::string> snapshot;
    std::vector<uint32_t> snapshotNodes;
    std::deque<Pending> pending;
    struct CachedNode { std::string document; double updated=0; };
    std::map<std::string, CachedNode> nodes;
    std::map<std::string, std::string> waypoints;
    size_t nodeCacheBytes() const;
    void cacheNode(const std::string &, const cJSON *);
    std::deque<uint64_t> seenPackets;
    std::deque<uint32_t> completedCommands;
    void connectWs();
    void disconnectWs();
    void finishDisconnect();
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
    void status(uint32_t, const char *, const char *encryption = nullptr, const char *errorCode = nullptr);
    void finishDiagnostic(const Pending &, bool, const meshtastic_MeshPacket * = nullptr,
                          const meshtastic_Telemetry * = nullptr);
    meshtastic_MeshPacket *packet(uint32_t, uint32_t = 0);
    bool transmit(meshtastic_MeshPacket *);
    uint64_t nowMs() const;
};
extern MvtWssGateway *mvtWssGateway;
#endif
