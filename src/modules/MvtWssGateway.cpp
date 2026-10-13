#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY
#include "MvtWssGateway.h"
#include "MvtPayload.h"
#include "MvtRootCA.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "PowerStatus.h"
#include "Router.h"
#include "RadioLibInterface.h"
#include "RTC.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"
#include <pb_decode.h>
#include <pb_encode.h>
#include <algorithm>
#include <cmath>
#include <memory>

MvtWssGateway *mvtWssGateway = nullptr;
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json json(cJSON *p) { return Json(p, cJSON_Delete); }
const cJSON *get(const cJSON *p, const char *key) { return cJSON_GetObjectItemCaseSensitive(p, key); }
const char *str(const cJSON *p, const char *key, const char *fallback = "") {
    const cJSON *v = get(p, key); return cJSON_IsString(v) ? v->valuestring : fallback;
}
bool jsonBool(const cJSON *p, const char *key) { return cJSON_IsTrue(get(p, key)); }
double number(const cJSON *p, const char *key, double fallback = 0) {
    const cJSON *v = get(p, key); return cJSON_IsNumber(v) && std::isfinite(v->valuedouble) ? v->valuedouble : fallback;
}
bool selectedWaypoint(const cJSON *wp, const cJSON *selection) {
    if (!(fabs(number(wp, "latitude", NAN) - number(selection, "latitude", NAN)) < 0.00005 &&
          fabs(number(wp, "longitude", NAN) - number(selection, "longitude", NAN)) < 0.00005)) return false;
    const std::string title = str(selection, "title");
    if (!title.empty() && title != str(wp, "title")) return false;
    double selectedTime = number(selection, "timestampSec"), storedTime = number(wp, "timestampSec");
    return !selectedTime || !storedTime || selectedTime == storedTime;
}
// Also accept Android's signed representation of uint32 node/packet IDs.
bool identifier(const cJSON *p, const char *key, uint32_t &out, uint32_t fallback = 0) {
    const cJSON *v = get(p, key);
    if (!v) { out = fallback; return true; }
    if (!cJSON_IsNumber(v) || !std::isfinite(v->valuedouble) || floor(v->valuedouble) != v->valuedouble ||
        v->valuedouble < -2147483648.0 || v->valuedouble > 4294967295.0) return false;
    out = uint32_t(int64_t(v->valuedouble)); return true;
}
std::string hex(uint32_t n) { char buf[9]; snprintf(buf, sizeof(buf), "%08x", n); return buf; }
std::string key(uint32_t n, bool mv = false) { return "!" + hex(n) + (mv ? "_mv" : ""); }
std::string serialize(cJSON *p) {
    char *s = cJSON_PrintUnformatted(p); if (!s) return "";
    std::string result(s); cJSON_free(s); return result;
}
cJSON *event(const char *type) {
    cJSON *p = cJSON_CreateObject(); cJSON_AddStringToObject(p, "type", type); return p;
}
void setNumber(cJSON *p, const char *k, double value) {
    cJSON_DeleteItemFromObjectCaseSensitive(p, k); cJSON_AddNumberToObject(p, k, value);
}
void setString(cJSON *p, const char *k, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(p, k); cJSON_AddStringToObject(p, k, value);
}
void setBool(cJSON *p, const char *k, bool value) {
    cJSON_DeleteItemFromObjectCaseSensitive(p, k); cJSON_AddBoolToObject(p, k, value);
}
bool decodeProto(const meshtastic_MeshPacket &p, const pb_msgdesc_t *fields, void *dest) {
    pb_istream_t stream = pb_istream_from_buffer(p.decoded.payload.bytes, p.decoded.payload.size);
    return pb_decode(&stream, fields, dest);
}
void deviceMetrics(cJSON *n, const meshtastic_DeviceMetrics &m) {
    if (m.has_battery_level) {
        setNumber(n, "radioBattery", std::min(uint32_t(100), m.battery_level));
        setNumber(n, "battery", std::min(uint32_t(100), m.battery_level));
        setBool(n, "radioCharging", m.battery_level > 100);
    }
    if (m.has_voltage) setNumber(n, "voltage", m.voltage);
    if (m.has_channel_utilization) setNumber(n, "channelUtilization", m.channel_utilization);
    if (m.has_air_util_tx) setNumber(n, "airUtilTx", m.air_util_tx);
}
void environmentMetrics(cJSON *n, const meshtastic_EnvironmentMetrics &m, bool result = false) {
    if (m.has_temperature) setNumber(n, result ? "temperature" : "temp", m.temperature);
    if (m.has_relative_humidity) setNumber(n, "humidity", m.relative_humidity);
    if (m.has_barometric_pressure) setNumber(n, "pressure", m.barometric_pressure);
    if (m.has_iaq) setNumber(n, "iaq", m.iaq);
}
void airMetrics(cJSON *n, const meshtastic_AirQualityMetrics &m, bool result = false) {
    if (m.has_co2_temperature) setNumber(n, result ? "temperature" : "temp", m.co2_temperature);
    if (m.has_co2_humidity) setNumber(n, "humidity", m.co2_humidity);
    if (m.has_pm_temperature) setNumber(n, result ? "temperature" : "temp", m.pm_temperature);
    if (m.has_pm_humidity) setNumber(n, "humidity", m.pm_humidity);
    if (m.has_co2) setNumber(n, "co2", m.co2);
}
const char *regionName(int value) {
    switch (value) {
    case meshtastic_Config_LoRaConfig_RegionCode_UNSET: return "UNSET";
    case meshtastic_Config_LoRaConfig_RegionCode_US: return "US";
    case meshtastic_Config_LoRaConfig_RegionCode_EU_433: return "EU_433";
    case meshtastic_Config_LoRaConfig_RegionCode_EU_868: return "EU_868";
    case meshtastic_Config_LoRaConfig_RegionCode_CN: return "CN";
    case meshtastic_Config_LoRaConfig_RegionCode_JP: return "JP";
    case meshtastic_Config_LoRaConfig_RegionCode_ANZ: return "ANZ";
    case meshtastic_Config_LoRaConfig_RegionCode_KR: return "KR";
    case meshtastic_Config_LoRaConfig_RegionCode_TW: return "TW";
    case meshtastic_Config_LoRaConfig_RegionCode_RU: return "RU";
    case meshtastic_Config_LoRaConfig_RegionCode_IN: return "IN";
    case meshtastic_Config_LoRaConfig_RegionCode_NZ_865: return "NZ_865";
    case meshtastic_Config_LoRaConfig_RegionCode_TH: return "TH";
    case meshtastic_Config_LoRaConfig_RegionCode_LORA_24: return "LORA_24";
    case meshtastic_Config_LoRaConfig_RegionCode_UA_433: return "UA_433";
    case meshtastic_Config_LoRaConfig_RegionCode_UA_868: return "UA_868";
    case meshtastic_Config_LoRaConfig_RegionCode_MY_433: return "MY_433";
    case meshtastic_Config_LoRaConfig_RegionCode_MY_919: return "MY_919";
    case meshtastic_Config_LoRaConfig_RegionCode_SG_923: return "SG_923";
    case meshtastic_Config_LoRaConfig_RegionCode_PH_433: return "PH_433";
    case meshtastic_Config_LoRaConfig_RegionCode_PH_868: return "PH_868";
    case meshtastic_Config_LoRaConfig_RegionCode_PH_915: return "PH_915";
    case meshtastic_Config_LoRaConfig_RegionCode_ANZ_433: return "ANZ_433";
    case meshtastic_Config_LoRaConfig_RegionCode_KZ_433: return "KZ_433";
    case meshtastic_Config_LoRaConfig_RegionCode_KZ_863: return "KZ_863";
    case meshtastic_Config_LoRaConfig_RegionCode_NP_865: return "NP_865";
    case meshtastic_Config_LoRaConfig_RegionCode_BR_902: return "BR_902";
    default: return "UNKNOWN";
    }
}
const char *presetName(int value) {
    switch (value) {
    case meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST: return "LONG_FAST";
    case meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW: return "LONG_SLOW";
    case meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW: return "VERY_LONG_SLOW";
    case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW: return "MEDIUM_SLOW";
    case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST: return "MEDIUM_FAST";
    case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW: return "SHORT_SLOW";
    case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST: return "SHORT_FAST";
    case meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE: return "LONG_MODERATE";
    case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO: return "SHORT_TURBO";
    default: return "UNKNOWN";
    }
}
const size_t MAX_FRAME = 16384, MAX_NODES = 96, MAX_EVENTS = 32, MAX_PENDING = 16;
const uint32_t ACK_TIMEOUT = 120000;
}

MvtWssGateway::MvtWssGateway() : MeshModule("mvt_wss"), concurrency::OSThread("mvt_wss", 100) {
    mvtWssGateway = this;
    isPromiscuous = true; encryptedOk = false; loopbackOk = true;
    incoming = xQueueCreate(8, sizeof(char *));
}
MvtWssGateway::~MvtWssGateway() {
    disconnectWs();
    if (incoming) vQueueDelete(incoming);
    mvtWssGateway = nullptr;
}
bool MvtWssGateway::wantPacket(const meshtastic_MeshPacket *p) {
    return p && p->which_payload_variant == meshtastic_MeshPacket_decoded_tag;
}
const char *MvtWssGateway::displayStatus() const {
    if (!moduleConfig.mqtt.enabled) return "OFF";
    if (!moduleConfig.mqtt.username[0]) return "SETUP";
    if (!moduleConfig.mqtt.password[0]) return "SETKEY";
    if (displayError) return displayError;
    if (inputFault) return "FRAME";
    if (WiFi.status() != WL_CONNECTED) return "WAIT";
    if (authenticated && socketConnected) return "OK";
    return socketConnected ? "AUTH" : "RETRY";
}
uint64_t MvtWssGateway::nowMs() const {
    uint32_t t = getValidTime(RTCQualityFromNet);
    if (t >= 1577836800) return uint64_t(t) * 1000;
    return serverTimeMs ? serverTimeMs + uint32_t(millis() - serverTimeTick) : 0;
}
void MvtWssGateway::handleWsEvent(void *args, esp_event_base_t, int32_t id, void *data) {
    auto *self = static_cast<MvtWssGateway *>(args);
    if (!self) return;
    if (id == WEBSOCKET_EVENT_CONNECTED) self->socketConnected = true;
    else if (id == WEBSOCKET_EVENT_DISCONNECTED || id == WEBSOCKET_EVENT_CLOSED) self->socketConnected = false;
    else if (id == WEBSOCKET_EVENT_DATA && data) self->receiveFrame(*static_cast<esp_websocket_event_data_t *>(data));
}
void MvtWssGateway::receiveFrame(const esp_websocket_event_data_t &d) {
    // IDF splits large frames into buffer-sized events. Control frames must
    // never be interpreted as JSON or appended to an unfinished text frame.
    if (d.op_code == 8 && d.data_ptr && d.data_len >= 2) {
        uint16_t code = (uint16_t(uint8_t(d.data_ptr[0])) << 8) | uint8_t(d.data_ptr[1]);
        if (code == 4001 || code == 4401 || code == 4403 || code == 4404) peerRejected = true;
    }
    if (d.op_code != 1 && d.op_code != 0) return;
    if (d.payload_len <= 0 || size_t(d.payload_len) > MAX_FRAME || d.payload_offset < 0 || d.data_len < 0) {
        inputFault = true; return;
    }
    if (d.payload_offset == 0) {
        if (!frame.empty()) { inputFault = true; return; }
        frameExpected = d.payload_len; frame.reserve(frameExpected);
    }
    if (frameExpected != size_t(d.payload_len) || frame.size() != size_t(d.payload_offset) ||
        size_t(d.data_len) > frameExpected - frame.size() || (!d.data_ptr && d.data_len)) {
        inputFault = true; return;
    }
    if (d.data_len && memchr(d.data_ptr, 0, d.data_len)) { inputFault = true; return; }
    if (d.data_len) frame.append(d.data_ptr, d.data_len);
    if (frame.size() == frameExpected) {
        char *copy = static_cast<char *>(malloc(frame.size() + 1));
        if (copy) {
            memcpy(copy, frame.data(), frame.size()); copy[frame.size()] = 0;
            if (!incoming || xQueueSend(incoming, &copy, 0) != pdTRUE) { free(copy); inputFault = true; }
        } else inputFault = true;
        frame.clear(); frameExpected = 0;
    }
}
void MvtWssGateway::disconnectWs() {
    if (wsClient) {
        esp_websocket_client_stop(wsClient); esp_websocket_client_destroy(wsClient); wsClient = nullptr;
    }
    socketConnected = false; authenticated = false; authSent = false;
    snapshot.clear();
    frame.clear(); frameExpected = 0; inputFault = false;
    char *p;
    while (incoming && xQueueReceive(incoming, &p, 0) == pdTRUE) free(p);
}
void MvtWssGateway::connectWs() {
    disconnectWs();
    std::string url = configuredAddress.empty() ? "wss://movetain.app/ws" : configuredAddress;
    if (url.find("://") == std::string::npos) url = "wss://" + url;
    size_t hostEnd = url.find('/', url.find("://") + 3);
    if (hostEnd == std::string::npos) url += "/ws";
    // Relay keys must never be sent in cleartext.
    if (url.compare(0, 6, "wss://") != 0) { authBlocked = true; displayError = "BADURL"; LOG_WARN("MVT requires wss://"); return; }
    esp_websocket_client_config_t cfg = {};
    cfg.uri = url.c_str(); cfg.cert_pem = MVT_ROOT_CA;
    cfg.task_stack = 6144; cfg.buffer_size = 2048;
    cfg.ping_interval_sec = 20; cfg.pingpong_timeout_sec = 45;
    cfg.disable_auto_reconnect = true;
    wsClient = esp_websocket_client_init(&cfg);
    if (!wsClient) return;
    esp_websocket_register_events(wsClient, WEBSOCKET_EVENT_ANY, handleWsEvent, this);
    if (esp_websocket_client_start(wsClient) != ESP_OK) disconnectWs();
}
bool MvtWssGateway::sendDocument(cJSON *p) {
    std::string text = serialize(p);
    if (!socketConnected || !wsClient || inputFault || text.empty()) return false;
    int written = esp_websocket_client_send_text(wsClient, text.c_str(), text.size(), pdMS_TO_TICKS(20));
    // Do not keep writing on a potentially partial websocket frame.
    if (written != int(text.size())) inputFault = true;
    return written == int(text.size());
}
void MvtWssGateway::publish(cJSON *p) {
    auto owned = json(p);
    std::string text = serialize(p);
    if (text.empty()) return;
    if (outgoing.size() == MAX_EVENTS) {
        outgoing.pop_front(); LOG_WARN("MVT offline event buffer full; oldest event dropped");
    }
    outgoing.push_back(std::move(text));
}
cJSON *MvtWssGateway::gatewayInfo() {
    uint32_t id = nodeDB->getNodeNum();
    cJSON *p = nodeDocument(id);
    setString(p, "nodeName", owner.long_name[0] ? owner.long_name : owner.short_name);
    setString(p, "longName", owner.long_name);
    setString(p, "shortName", owner.short_name);
    setString(p, "gatewayType", "WSS");
    auto *radio = RadioLibInterface::instance;
    if (radio && radio->getFreq() > 0) {
        char frequency[24]; snprintf(frequency, sizeof(frequency), "%.3f MHz", double(radio->getFreq()));
        setString(p, "frequency", frequency);
    }
    char channel[48]; snprintf(channel, sizeof(channel), "%s (Ch %u)", channels.getName(channels.getPrimaryIndex()),
                               unsigned(channels.getPrimaryIndex()));
    setString(p, "channelName", channel);
    setString(p, "region", regionName(config.lora.region));
    setString(p, "modemPreset", config.lora.use_preset ? presetName(config.lora.modem_preset) : "CUSTOM");
    setString(p, "transport", "Direct WSS (WiFi)");
    setString(p, "firmwareVersion", xstr(APP_VERSION));
    setBool(p, "radioConnected", radio && radio->getFreq() > 0);
    setBool(p, "txEnabled", config.lora.tx_enabled);
    setNumber(p, "radioBattery", powerStatus ? powerStatus->getBatteryChargePercent() : 0);
    setNumber(p, "voltage", powerStatus ? powerStatus->getBatteryVoltageMv() / 1000.0 : 0);
    setNumber(p, "hopLimit", portalHopLimit ? portalHopLimit : config.lora.hop_limit);
    setNumber(p, "radioHopLimit", config.lora.hop_limit);
    setBool(p, "hopLimitControl", true);
    return p;
}
void MvtWssGateway::sendAuthGateway() {
    auto p = json(event("AUTH_GATEWAY"));
    cJSON_AddStringToObject(p.get(), "room", configuredRoom.c_str());
    cJSON_AddStringToObject(p.get(), "relayKey", configuredKey.c_str());
    cJSON_AddItemToObject(p.get(), "gatewayInfo", gatewayInfo());
    authSent = sendDocument(p.get()); authStartedMs = millis();
}
void MvtWssGateway::sendHeartbeat() {
    auto p = json(event("GATEWAY_HEARTBEAT"));
    cJSON_AddNumberToObject(p.get(), "timestamp", double(nowMs()));
    cJSON_AddItemToObject(p.get(), "gatewayInfo", gatewayInfo());
    if (sendDocument(p.get())) lastHeartbeatMs = millis();
}
cJSON *MvtWssGateway::nodeDocument(uint32_t id, bool mv) {
    std::string k = key(id, mv);
    auto found = nodes.find(k);
    cJSON *p = found == nodes.end() ? cJSON_CreateObject() : cJSON_Parse(found->second.c_str());
    if (!p) p = cJSON_CreateObject();
    setString(p, "nodeKey", k.c_str()); setNumber(p, "nodeId", id); setString(p, "nodeHex", hex(id).c_str());
    auto *n = nodeDB->getMeshNode(id);
    if (n && !mv) {
        if (n->has_user && n->user.short_name[0]) setString(p, "callsign", n->user.short_name);
        if (n->has_user && n->user.long_name[0]) setString(p, "longName", n->user.long_name);
        if (n->has_user && n->user.short_name[0]) setString(p, "shortName", n->user.short_name);
        if (n->has_position && (n->position.latitude_i || n->position.longitude_i)) {
            setNumber(p, "lat", n->position.latitude_i / 1e7); setNumber(p, "lon", n->position.longitude_i / 1e7);
            setNumber(p, "alt", n->position.altitude);
        }
        if (n->has_device_metrics) deviceMetrics(p, n->device_metrics);
        if (n->has_hops_away) setNumber(p, "hops", n->hops_away);
        setNumber(p, "snr", n->snr);
        if (n->last_heard >= 1577836800) setNumber(p, "lastUpdateMs", double(uint64_t(n->last_heard) * 1000));
    }
    if (!get(p, "callsign")) setString(p, "callsign", hex(id).c_str());
    return p;
}
void MvtWssGateway::publishNode(cJSON *p) {
    auto owned = json(p);
    std::string k = str(p, "nodeKey");
    if (nodes.size() >= MAX_NODES && !nodes.count(k)) {
        // Cache only virtual nodes; standard nodes can be rebuilt from NodeDB.
        auto victim = std::min_element(nodes.begin(), nodes.end(), [](const std::pair<const std::string, std::string> &a,
                                                                    const std::pair<const std::string, std::string> &b) {
            auto ja = json(cJSON_Parse(a.second.c_str())), jb = json(cJSON_Parse(b.second.c_str()));
            return number(ja.get(), "lastUpdateMs") < number(jb.get(), "lastUpdateMs");
        });
        if (victim != nodes.end()) nodes.erase(victim);
    }
    nodes[k] = serialize(p);
    if (!authenticated || !nowMs()) return; // Reconnect snapshot contains latest state.
    auto e = json(event("NODE_UPDATE"));
    cJSON_AddStringToObject(e.get(), "nodeKey", k.c_str());
    cJSON_AddItemToObject(e.get(), "node", owned.release());
    if (!sendDocument(e.get())) snapshotPending = true;
}
void MvtWssGateway::publishWaypoint(cJSON *p) {
    auto owned = json(p);
    std::string id = std::to_string(number(p, "latitude")) + ":" + std::to_string(number(p, "longitude")) + ":" + str(p, "title") + ":" + std::to_string(number(p, "timestampSec"));
    if (waypoints.size() >= 64) waypoints.erase(waypoints.begin());
    waypoints[id] = serialize(p);
    auto e = event("WAYPOINT"); cJSON_AddItemToObject(e, "waypoint", owned.release()); publish(e);
}
void MvtWssGateway::sendSnapshot() {
    // Send updates individually to avoid a large heap allocation and avoid
    // deleting portal nodes when the gateway's bounded cache is incomplete.
    if (snapshot.empty()) {
    LOG_INFO("MVT snapshot NodeDB entries=%u", unsigned(nodeDB->getNumMeshNodes()));
    for (size_t i = 0; i < nodeDB->getNumMeshNodes(); ++i) {
        auto *n = nodeDB->getMeshNodeByIndex(i);
        if (n->num == nodeDB->getNodeNum() || n->is_ignored) continue;
        snapshot.push_back("N" + key(n->num));
    }
    for (const auto &n : nodes) {
        if (n.first == key(nodeDB->getNodeNum())) continue;
        if (n.first.find("_mv") == std::string::npos && nodeDB->getMeshNode(strtoul(n.first.c_str()+1, nullptr, 16))) continue;
        snapshot.push_back("C" + n.first);
    }
    for (const auto &w : waypoints) {
        snapshot.push_back("W" + w.first);
    }
    }
    // At most one snapshot item per turn so LoRa processing stays responsive.
    if (!snapshot.empty()) {
        const std::string &item = snapshot.front();
        auto e = json(event(item[0] == 'W' ? "WAYPOINT" : "NODE_UPDATE"));
        cJSON_AddBoolToObject(e.get(), "snapshot", true);
        if (item[0] == 'N')
            cJSON_AddItemToObject(e.get(), "node", nodeDocument(strtoul(item.c_str() + 2, nullptr, 16)));
        else {
            const auto &cache = item[0] == 'W' ? waypoints : nodes;
            auto found = cache.find(item.substr(1));
            if (found == cache.end()) { snapshot.pop_front(); return; }
            cJSON_AddItemToObject(e.get(), item[0] == 'W' ? "waypoint" : "node", cJSON_Parse(found->second.c_str()));
        }
        cJSON *n = cJSON_GetObjectItemCaseSensitive(e.get(), "node");
        // A NodeDB snapshot is historical, not evidence of a fresh RF reply.
        if (n && !get(n, "lastUpdateMs")) setNumber(n, "lastUpdateMs", 0);
        if (sendDocument(e.get())) snapshot.pop_front();
    }
    snapshotPending = !snapshot.empty();
}
void MvtWssGateway::status(uint32_t id, const char *value) {
    if (!id) return;
    auto p = event("CHAT_STATUS_UPDATE"); cJSON_AddNumberToObject(p, "id", id);
    cJSON_AddStringToObject(p, "status", value); publish(p);
}
meshtastic_MeshPacket *MvtWssGateway::packet(uint32_t target, uint32_t id) {
    if (!router || !target || target == 1 || !config.lora.tx_enabled) return nullptr;
    auto *p = router->allocForSending(); if (!p) return nullptr;
    p->to = target; p->from = nodeDB->getNodeNum(); if (id) p->id = id;
    p->channel = target == NODENUM_BROADCAST ? channels.getPrimaryIndex() : nodeDB->getMeshNodeChannel(target);
    p->hop_limit = portalHopLimit ? portalHopLimit : config.lora.hop_limit; p->hop_start = p->hop_limit;
    p->want_ack = target != NODENUM_BROADCAST;
    return p;
}
bool MvtWssGateway::transmit(meshtastic_MeshPacket *p) {
    if (!p) return false;
    // Router owns/frees p even on failure. Do not report radio ACK at dispatch.
    return router->sendLocal(p, RX_SRC_LOCAL) == ERRNO_OK;
}
void MvtWssGateway::finishDiagnostic(const Pending &p, bool success, const meshtastic_MeshPacket *mp,
                                   const meshtastic_Telemetry *telemetry) {
    auto e = event(p.kind == "trace" ? (success ? "TRACEROUTE_RESULT" : "TRACEROUTE_TIMEOUT") :
                   p.kind == "env" ? "POLL_ENV_RESULT" : "PING_RESULT");
    cJSON_AddNumberToObject(e, "targetNodeId", p.target); cJSON_AddStringToObject(e, "targetNodeHex", hex(p.target).c_str());
    cJSON_AddStringToObject(e, "nodeKey", p.nodeKey.c_str()); cJSON_AddBoolToObject(e, "isVirtual", p.isVirtual);
    cJSON_AddBoolToObject(e, "success", success);
    if (p.kind == "ping") {
        cJSON_AddBoolToObject(e, "positionReceived", p.positionReceived);
        cJSON_AddBoolToObject(e, "telemetryReceived", p.telemetryReceived);
        cJSON_AddBoolToObject(e, "nodeInfoReceived", p.nodeInfoReceived);
    }
    if (success) cJSON_AddNumberToObject(e, "rttMs", uint32_t(millis() - p.started));
    if (mp) {
        cJSON_AddNumberToObject(e, "snr", mp->rx_snr);
        cJSON_AddNumberToObject(e, "hops", mp->hop_start >= mp->hop_limit ? mp->hop_start - mp->hop_limit : 0);
    }
    if (telemetry && telemetry->which_variant == meshtastic_Telemetry_device_metrics_tag &&
        telemetry->variant.device_metrics.has_battery_level)
        cJSON_AddNumberToObject(e, "batteryPct", std::min(uint32_t(100), telemetry->variant.device_metrics.battery_level));
    if (telemetry && telemetry->which_variant == meshtastic_Telemetry_environment_metrics_tag)
        environmentMetrics(e, telemetry->variant.environment_metrics, true);
    if (telemetry && telemetry->which_variant == meshtastic_Telemetry_air_quality_metrics_tag)
        airMetrics(e, telemetry->variant.air_quality_metrics, true);
    if (p.kind == "trace" && mp && success) {
        meshtastic_RouteDiscovery r = meshtastic_RouteDiscovery_init_default;
        if (decodeProto(*mp, &meshtastic_RouteDiscovery_msg, &r)) {
            auto addRoute = [&](const char *name, const uint32_t *ids, size_t count, const int8_t *snr, size_t snrCount) {
                cJSON *a = cJSON_AddArrayToObject(e, name);
                for (size_t i = 0; i < count; ++i) {
                    cJSON *h = cJSON_CreateObject(); cJSON_AddNumberToObject(h, "nodeId", ids[i]);
                    cJSON_AddStringToObject(h, "nodeHex", hex(ids[i]).c_str());
                    auto *n = nodeDB->getMeshNode(ids[i]);
                    cJSON_AddStringToObject(h, "callsign", n && n->has_user ? (n->user.long_name[0] ? n->user.long_name : n->user.short_name) : hex(ids[i]).c_str());
                    if (i < snrCount && snr[i] != INT8_MIN) cJSON_AddNumberToObject(h, "snr", snr[i] / 4.0);
                    cJSON_AddItemToArray(a, h);
                }
            };
            addRoute("outbound", r.route, r.route_count, r.snr_towards, r.snr_towards_count);
            addRoute("returnRoute", r.route_back, r.route_back_count, r.snr_back, r.snr_back_count);
            if (r.snr_towards_count > r.route_count && r.snr_towards[r.route_count] != INT8_MIN)
                cJSON_AddNumberToObject(e, "outboundLastSnr", r.snr_towards[r.route_count] / 4.0);
            if (r.snr_back_count > r.route_back_count && r.snr_back[r.route_back_count] != INT8_MIN)
                cJSON_AddNumberToObject(e, "returnLastSnr", r.snr_back[r.route_back_count] / 4.0);
        }
    }
    publish(e);
}
ProcessMessage MvtWssGateway::handleReceived(const meshtastic_MeshPacket &mp) {
    if (!wantPacket(&mp) || mp.decoded.payload.size > sizeof(mp.decoded.payload.bytes)) return ProcessMessage::CONTINUE;
    uint32_t from = mp.from ? mp.from : nodeDB->getNodeNum();
    bool local = from == nodeDB->getNodeNum();
    uint32_t request = mp.decoded.request_id;
    meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_default;
    bool hasTelemetry = mp.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP &&
        decodeProto(mp, &meshtastic_Telemetry_msg, &telemetry);
    mvt::Payload mv;
    bool hasMv = mp.decoded.portnum == meshtastic_PortNum_PRIVATE_APP &&
        mvt::decode(mp.decoded.payload.bytes, mp.decoded.payload.size, mv);
    for (auto &poll : polls) {
        if (from != poll.command.target) continue;
        for (unsigned i = 0; i < poll.sent; ++i) {
            const unsigned kind = poll.needsInfo ? i : i + 1;
            bool correlated = request == poll.ids[i] && mp.to == nodeDB->getNodeNum();
            if (poll.command.isVirtual && hasMv && !(mv.flags & (1 | 2 | 8))) correlated = true;
            if (!correlated || (poll.replies & (1 << i))) continue;
            bool valid = false;
            if (kind == 0 && mp.decoded.portnum == meshtastic_PortNum_NODEINFO_APP) {
                meshtastic_User user = meshtastic_User_init_default;
                valid = decodeProto(mp, &meshtastic_User_msg, &user);
                if (valid) poll.command.nodeInfoReceived = true;
            } else if (kind == 1 && (mp.decoded.portnum == meshtastic_PortNum_POSITION_APP || hasMv)) {
                meshtastic_Position pos = meshtastic_Position_init_default;
                valid = hasMv || decodeProto(mp, &meshtastic_Position_msg, &pos);
                if (valid) poll.command.positionReceived = true;
            } else if (kind == 2 && hasTelemetry && telemetry.which_variant == meshtastic_Telemetry_device_metrics_tag) {
                valid = true; poll.command.telemetryReceived = true;
            }
            if (valid) {
                poll.replies |= 1 << i;
                {
                    // Report a genuine correlated response promptly; remaining requests still run.
                    finishDiagnostic(poll.command, true, &mp, hasTelemetry ? &telemetry : nullptr);
                }
            }
        }
    }
    // Delivery proof must match request ID AND the intended peer, never a
    // relay's implicit ACK or our own local router acknowledgement.
    for (auto it = pending.begin(); it != pending.end();) {
        bool matches = request == it->id && from == it->target && mp.to == nodeDB->getNodeNum();
        bool unsolicitedMv = it->isVirtual && it->kind == "ping" && hasMv && from == it->target &&
            !(mv.flags & (1 | 2 | 8));
        const bool chatAck = it->kind == "chat" || it->kind == "latechat";
        if (chatAck && request == it->id && mp.decoded.portnum == meshtastic_PortNum_ROUTING_APP) {
            meshtastic_Routing routing = meshtastic_Routing_init_default;
            if (decodeProto(mp, &meshtastic_Routing_msg, &routing) && routing.which_variant == meshtastic_Routing_error_reason_tag) {
                bool delivered = matches && routing.error_reason == meshtastic_Routing_Error_NONE;
                bool failed = (matches || local) && routing.error_reason != meshtastic_Routing_Error_NONE;
                if (delivered || failed) { status(it->id, delivered ? "DELIVERED" : "FAILED"); it = pending.erase(it); continue; }
            }
        } else if (!chatAck && (matches || unsolicitedMv)) {
            bool valid = it->kind == "trace" ? mp.decoded.portnum == meshtastic_PortNum_TRACEROUTE_APP :
                it->kind == "env" ? hasTelemetry && (telemetry.which_variant == meshtastic_Telemetry_environment_metrics_tag ||
                                                      telemetry.which_variant == meshtastic_Telemetry_air_quality_metrics_tag) :
                hasMv || (hasTelemetry && telemetry.which_variant == meshtastic_Telemetry_device_metrics_tag);
            if (valid) {
                if (it->kind == "trace") {
                    meshtastic_RouteDiscovery route = meshtastic_RouteDiscovery_init_default;
                    valid = decodeProto(mp, &meshtastic_RouteDiscovery_msg, &route);
                }
                if (valid) {
                    finishDiagnostic(*it, true, &mp, hasTelemetry ? &telemetry : nullptr);
                    it = pending.erase(it); continue;
                }
            }
        }
        ++it;
    }
    if (local && hasTelemetry && !mp.decoded.want_response) {
        auto n = json(nodeDocument(from));
        if (telemetry.which_variant == meshtastic_Telemetry_environment_metrics_tag)
            environmentMetrics(n.get(), telemetry.variant.environment_metrics);
        if (telemetry.which_variant == meshtastic_Telemetry_air_quality_metrics_tag)
            airMetrics(n.get(), telemetry.variant.air_quality_metrics);
        nodes[key(from)] = serialize(n.get());
        if (authenticated) sendHeartbeat();
    }
    if (local || mp.decoded.want_response) return ProcessMessage::CONTINUE;
    uint64_t identity = (uint64_t(from) << 32) | mp.id;
    if (std::find(seenPackets.begin(), seenPackets.end(), identity) != seenPackets.end()) return ProcessMessage::CONTINUE;
    if (seenPackets.size() >= 128) seenPackets.pop_front();
    seenPackets.push_back(identity);
    uint32_t hops = mp.hop_start >= mp.hop_limit ? mp.hop_start - mp.hop_limit : 0;
    auto n = json(nodeDocument(from, hasMv));
    setNumber(n.get(), "lastUpdateMs", double(nowMs())); setNumber(n.get(), "snr", mp.rx_snr);
    setNumber(n.get(), "rssi", mp.rx_rssi); setNumber(n.get(), "hops", hops);
    if (hasMv) {
        if (mv.flags & 8) {
            std::string k = key(from, true); nodes.erase(k);
            auto e = event("NODE_REMOVE"); cJSON_AddStringToObject(e, "nodeKey", k.c_str()); publish(e);
            return ProcessMessage::CONTINUE;
        }
        setBool(n.get(), "isMovetain", true); setBool(n.get(), "isBeaconActive", !(mv.flags & (1 | 2)));
        setString(n.get(), "callsign", mv.name.empty() ? hex(from).c_str() : mv.name.c_str());
        setString(n.get(), "mvColor", mvt::colors[mv.color < 9 ? mv.color : 0]);
        setString(n.get(), "mvIcon", mvt::icons[mv.icon < 6 ? mv.icon : 0]);
        setNumber(n.get(), "phoneBattery", mv.battery); setNumber(n.get(), "battery", mv.battery);
        setBool(n.get(), "phoneCharging", mv.charging);
        if (!(mv.flags & 2)) { setNumber(n.get(), "lat", mv.lat); setNumber(n.get(), "lon", mv.lon); }
        if (mv.flags & 16) setNumber(n.get(), "alt", mv.altitude);
        if (mv.flags & 32) setNumber(n.get(), "heartRate", mv.heartRate);
        if (!(mv.flags & (1 | 2))) publishNode(n.release());
        if (mv.flags & (1 | 2 | 4)) {
            auto msg = cJSON_CreateObject();
            cJSON_AddNumberToObject(msg, "id", mp.id); cJSON_AddNumberToObject(msg, "senderId", from);
            cJSON_AddNumberToObject(msg, "recipientId", mp.to);
            cJSON_AddStringToObject(msg, "senderCallsign", mv.name.c_str());
            cJSON_AddStringToObject(msg, "text", mv.comment.c_str());
            cJSON_AddNumberToObject(msg, "timestampMs", double(nowMs()));
            cJSON_AddBoolToObject(msg, "isSos", mv.flags & 4); cJSON_AddNumberToObject(msg, "hops", hops);
            cJSON_AddStringToObject(msg, "status", "RECEIVED");
            if ((mv.flags & (1 | 4)) && (mv.lat != 0 || mv.lon != 0)) {
                auto wp = cJSON_CreateObject();
                const bool sos = mv.flags & 4;
                const std::string title = !mv.comment.empty() && !(sos && (mv.comment == "SOS" || mv.comment == u8"СОС"))
                    ? mv.comment : sos ? "SOS Emergency" : mv.icon == 5 ? "Target Point" : "Waypoint";
                cJSON_AddStringToObject(wp, "title", title.c_str());
                cJSON_AddStringToObject(wp, "comment", ("From " + mv.name).c_str());
                cJSON_AddNumberToObject(wp, "latitude", mv.lat); cJSON_AddNumberToObject(wp, "longitude", mv.lon);
                cJSON_AddNumberToObject(wp, "altitude", mv.altitude); cJSON_AddNumberToObject(wp, "timestampSec", double(nowMs() / 1000));
                cJSON_AddBoolToObject(wp, "isRadio", true); cJSON_AddBoolToObject(wp, "isTarget", mv.icon == 5);
                cJSON_AddBoolToObject(wp, "isSos", sos);
                publishWaypoint(cJSON_Duplicate(wp, true)); cJSON_AddItemToObject(msg, "waypoint", wp);
            }
            auto e = event("CHAT_MESSAGE"); cJSON_AddItemToObject(e, "message", msg); publish(e);
        }
    } else if (mp.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
        auto msg = cJSON_CreateObject();
        std::string text(reinterpret_cast<const char *>(mp.decoded.payload.bytes), mp.decoded.payload.size);
        cJSON_AddNumberToObject(msg, "id", mp.id); cJSON_AddNumberToObject(msg, "senderId", from);
        cJSON_AddNumberToObject(msg, "recipientId", mp.to); cJSON_AddStringToObject(msg, "senderCallsign", str(n.get(), "callsign"));
        cJSON_AddStringToObject(msg, "text", text.c_str()); cJSON_AddNumberToObject(msg, "timestampMs", double(nowMs()));
        cJSON_AddBoolToObject(msg, "isSos", false); cJSON_AddStringToObject(msg, "status", "RECEIVED");
        cJSON_AddNumberToObject(msg, "hops", hops);
        auto e = event("CHAT_MESSAGE"); cJSON_AddItemToObject(e, "message", msg); publish(e);
    } else if (mp.decoded.portnum == meshtastic_PortNum_POSITION_APP) {
        meshtastic_Position p = meshtastic_Position_init_default;
        if (decodeProto(mp, &meshtastic_Position_msg, &p)) {
            const bool fix = p.has_latitude_i && p.has_longitude_i && (p.latitude_i || p.longitude_i);
            setBool(n.get(), "gpsFix", fix);
            if (fix) {
                setNumber(n.get(), "lat", p.latitude_i / 1e7); setNumber(n.get(), "lon", p.longitude_i / 1e7);
                setNumber(n.get(), "alt", p.altitude); setNumber(n.get(), "lastPositionMs", double(nowMs()));
            }
            publishNode(n.release());
        }
    } else if (hasTelemetry) {
        if (telemetry.which_variant == meshtastic_Telemetry_device_metrics_tag) deviceMetrics(n.get(), telemetry.variant.device_metrics);
        if (telemetry.which_variant == meshtastic_Telemetry_environment_metrics_tag) environmentMetrics(n.get(), telemetry.variant.environment_metrics);
        if (telemetry.which_variant == meshtastic_Telemetry_air_quality_metrics_tag) airMetrics(n.get(), telemetry.variant.air_quality_metrics);
        if (telemetry.which_variant == meshtastic_Telemetry_health_metrics_tag && telemetry.variant.health_metrics.has_heart_bpm)
            setNumber(n.get(), "heartRate", telemetry.variant.health_metrics.heart_bpm);
        publishNode(n.release());
    } else if (mp.decoded.portnum == meshtastic_PortNum_NODEINFO_APP) {
        meshtastic_User u = meshtastic_User_init_default;
        if (decodeProto(mp, &meshtastic_User_msg, &u)) {
            setString(n.get(), "callsign", u.short_name[0] ? u.short_name : hex(from).c_str());
            setString(n.get(), "longName", u.long_name); setString(n.get(), "shortName", u.short_name); publishNode(n.release());
        }
    } else if (mp.decoded.portnum == meshtastic_PortNum_WAYPOINT_APP) {
        meshtastic_Waypoint w = meshtastic_Waypoint_init_default;
        if (decodeProto(mp, &meshtastic_Waypoint_msg, &w)) {
            auto wp = cJSON_CreateObject(); cJSON_AddStringToObject(wp, "title", w.name);
            cJSON_AddStringToObject(wp, "comment", w.description); cJSON_AddNumberToObject(wp, "latitude", w.latitude_i / 1e7);
            cJSON_AddNumberToObject(wp, "longitude", w.longitude_i / 1e7);
            cJSON_AddNumberToObject(wp, "timestampSec", double(nowMs() / 1000));
            cJSON_AddBoolToObject(wp, "isRadio", true); cJSON_AddBoolToObject(wp, "isTarget", strstr(w.name, "Target") != nullptr);
            publishWaypoint(wp);
        }
    }
    return ProcessMessage::CONTINUE;
}

void MvtWssGateway::handleIncomingCommand(const char *text) {
    // Require a complete single JSON document, including no trailing garbage.
    const char *end = nullptr;
    auto j = json(cJSON_ParseWithOpts(text, &end, true));
    if (!j || !cJSON_IsObject(j.get())) { LOG_WARN("MVT rejected malformed JSON"); return; }
    const std::string type = str(j.get(), "type");
    if (type == "ERROR") {
        const std::string code = str(j.get(), "code");
        if (code == "NOT_AUTHENTICATED") { authenticated = false; authSent = false; return; }
        if (!authenticated || code == "GATEWAY_REPLACED" || code == "INVALID_RELAY_KEY" || code == "ADMIN_RESTRICTED")
            authBlocked = true;
        if (authBlocked) displayError = code == "INVALID_RELAY_KEY" ? "BADKEY" : code == "ROOM_NOT_FOUND" ? "NOROOM" :
            code == "GATEWAY_REPLACED" ? "TAKEN" : code == "ADMIN_RESTRICTED" ? "BLOCK" : "FAIL";
        return;
    }
    if (type == "AUTH_GATEWAY_SUCCESS") {
        double hops = number(j.get(), "wssHopLimit");
        portalHopLimit = hops >= 1 && hops <= 7 && floor(hops) == hops ? uint8_t(hops) : 0;
        if (!authSent) return;
        authenticated = true; displayError = nullptr; snapshotPending = true;
        LOG_INFO("MVT WSS authenticated"); lastHeartbeatMs = 0;
        // The ACK provides server time even on devices without GPS/NTP.
        sendHeartbeat(); return;
    }
    if (type == "GATEWAY_HEARTBEAT_ACK") {
        double t = number(j.get(), "timestamp");
        if (authenticated && t >= 1577836800000.0 && t < 4102444800000.0) {
            serverTimeMs = uint64_t(t); serverTimeTick = millis();
        }
        return;
    }
    if (!authenticated || authBlocked) return;
    if (type == "CMD_SET_WSS_HOP_LIMIT") {
        double hops = number(j.get(), "hopLimit");
        if (hops < 1 || hops > 7 || floor(hops) != hops) return;
        portalHopLimit = uint8_t(hops);
        sendHeartbeat(); return;
    }
    if (type == "CMD_REFRESH_GATEWAY") { sendHeartbeat(); snapshotPending = true; return; }
    if (type == "WAYPOINT_DELETED" || type == "CMD_DELETE_WAYPOINT") {
        for (auto it = waypoints.begin(); it != waypoints.end();) {
            auto wp = json(cJSON_Parse(it->second.c_str()));
            if (selectedWaypoint(wp.get(), j.get())) {
                waypoints.erase(it); break;
            }
            else ++it;
        }
        // Remove unsent/replay attachments as well, otherwise an offline
        // waypoint would be resurrected immediately after its deletion.
        for (auto it = outgoing.begin(); it != outgoing.end();) {
            auto e = json(cJSON_Parse(it->c_str()));
            cJSON *chat = cJSON_GetObjectItemCaseSensitive(e.get(), "message");
            cJSON *wp = cJSON_GetObjectItemCaseSensitive(chat ? chat : e.get(), "waypoint");
            if (wp && selectedWaypoint(wp, j.get())) {
                if (!chat) { it = outgoing.erase(it); continue; }
                cJSON_DeleteItemFromObjectCaseSensitive(chat, "waypoint"); *it = serialize(e.get());
            }
            ++it;
        }
        return; // Same as Android: remove local cache, no invented radio delete protocol.
    }
    uint32_t target = 0, id = 0;
    bool chat = type == "CMD_TX_LORA_CHAT", waypoint = type == "CMD_TX_LORA_WAYPOINT";
    if (chat || waypoint) {
        if (!identifier(j.get(), "recipientId", target, NODENUM_BROADCAST) || !target || target == 1 ||
            !identifier(j.get(), "id", id)) return;
        if (id && (std::find(completedCommands.begin(), completedCommands.end(), id) != completedCommands.end() ||
            std::any_of(pending.begin(), pending.end(), [id](const Pending &p) { return p.id == id; }))) return;
        const size_t active = std::count_if(pending.begin(), pending.end(), [](const Pending &p) { return p.kind != "latechat"; });
        if (active >= MAX_PENDING && chat && target != NODENUM_BROADCAST) { status(id, "FAILED"); return; }
        auto *p = packet(target, id);
        if (!p) { if (chat) status(id, "FAILED"); return; }
        const cJSON *wp = get(j.get(), "waypoint");
        mvt::Payload payload;
        payload.name = chat ? str(j.get(), "senderCallsign", "HQ") : str(wp, "title", "HQ Waypoint");
        payload.comment = chat ? str(j.get(), "text") : str(wp, "comment");
        payload.flags = chat ? 2 : 1;
        if (jsonBool(j.get(), "isSos")) payload.flags |= 4;
        if (waypoint) {
            payload.lat = number(wp, "latitude", NAN); payload.lon = number(wp, "longitude", NAN);
            payload.icon = jsonBool(wp, "isTarget") ? 5 : 3;
            double altitude = number(wp, "altitude");
            if (altitude >= -32768 && altitude <= 32767) { payload.altitude = int16_t(altitude); payload.flags |= 16; }
        }
        p->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
        p->decoded.payload.size = mvt::encode(payload, p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes));
        if (!p->decoded.payload.size || (chat && payload.comment.empty())) {
            packetPool.release(p); if (chat) status(id, "FAILED"); return;
        }
        uint32_t packetId = p->id;
        if (chat && target != NODENUM_BROADCAST)
            pending.push_back({packetId, target, millis(), "chat", key(target), false});
        if (!transmit(p)) {
            pending.erase(std::remove_if(pending.begin(), pending.end(), [packetId](const Pending &x) { return x.id == packetId; }), pending.end());
            if (chat) status(id, "FAILED");
            return;
        }
        if (chat) status(packetId, target == NODENUM_BROADCAST ? "SENT" : "AWAITING_ACK");
        if (id) { if (completedCommands.size() >= 64) completedCommands.pop_front(); completedCommands.push_back(id); }
        return;
    }
    if (type == "CMD_PING_NODE" || type == "CMD_POLL_ENV" || type == "CMD_TRACEROUTE") {
        if (!identifier(j.get(), "targetNodeId", target) || !target || target == 1 || target == NODENUM_BROADCAST) return;
        bool mv = jsonBool(j.get(), "isVirtual");
        std::string nodeKey = str(j.get(), "nodeKey", key(target, mv).c_str());
        std::string kind = type == "CMD_TRACEROUTE" ? "trace" : type == "CMD_POLL_ENV" ? "env" : "ping";
        const size_t active = std::count_if(pending.begin(), pending.end(), [](const Pending &p) { return p.kind != "latechat"; });
        if (active + polls.size() >= MAX_PENDING || std::any_of(polls.begin(), polls.end(), [target](const Poll &p) { return p.command.target == target; }) || std::any_of(pending.begin(), pending.end(), [target](const Pending &p) {
            return p.target == target && p.kind != "chat" && p.kind != "latechat";
        })) {
            auto e = event("COMMAND_THROTTLED"); cJSON_AddNumberToObject(e, "targetNodeId", target);
            cJSON_AddStringToObject(e, "nodeKey", nodeKey.c_str()); cJSON_AddNumberToObject(e, "remainingSec", 120); publish(e); return;
        }
        if (target == nodeDB->getNodeNum() && kind == "ping") {
            auto e = event("PING_RESULT"); cJSON_AddNumberToObject(e, "targetNodeId", target);
            cJSON_AddStringToObject(e, "targetNodeHex", hex(target).c_str()); cJSON_AddStringToObject(e, "nodeKey", nodeKey.c_str());
            cJSON_AddBoolToObject(e, "success", true); cJSON_AddNumberToObject(e, "rttMs", 0);
            cJSON_AddNumberToObject(e, "hops", 0); cJSON_AddNumberToObject(e, "batteryPct", powerStatus ? powerStatus->getBatteryChargePercent() : 0);
            publish(e); sendHeartbeat(); return;
        }
        if (kind == "ping") {
            auto *known = nodeDB->getMeshNode(target);
            Poll poll;
            poll.command = {0, target, millis(), "ping", nodeKey, mv};
            poll.needsInfo = !mv && (!known || !known->has_user ||
                (!known->user.long_name[0] && !known->user.short_name[0]));
            poll.count = mv ? 1 : poll.needsInfo ? 3 : 2;
            uint32_t base = config.lora.modem_preset == meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW ? 5500 :
                config.lora.modem_preset == meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW ? 3000 :
                config.lora.modem_preset == meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST ? 600 : 500;
            unsigned hops = known && known->has_hops_away ? std::max(1U, unsigned(known->hops_away)) : config.lora.hop_limit;
            poll.spacing = std::min(25000U, std::max(3000U, base * std::min(hops, 6U) * 2 + 1500));
            poll.nextAt = millis(); polls.push_back(poll); advancePolls();
            return;
        }
        auto *p = packet(target);
        Pending q = {p ? p->id : 0, target, millis(), kind, nodeKey, mv};
        if (!p) { finishDiagnostic(q, false); return; }
        p->decoded.want_response = true;
        if (kind == "trace") {
            p->decoded.portnum = meshtastic_PortNum_TRACEROUTE_APP;
        } else if (mv && kind == "ping") {
            // Android auto-replies to POSITION_APP requests with a fresh MV beacon.
            p->decoded.portnum = meshtastic_PortNum_POSITION_APP;
        } else {
            meshtastic_Telemetry t = meshtastic_Telemetry_init_default;
            t.time = uint32_t(nowMs() / 1000);
            t.which_variant = kind == "env" ? meshtastic_Telemetry_environment_metrics_tag : meshtastic_Telemetry_device_metrics_tag;
            p->decoded.portnum = meshtastic_PortNum_TELEMETRY_APP;
            p->decoded.payload.size = pb_encode_to_bytes(p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes), &meshtastic_Telemetry_msg, &t);
        }
        pending.push_back(q);
        if (!transmit(p)) { pending.pop_back(); finishDiagnostic(q, false); }
    }
}
void MvtWssGateway::advancePolls() {
    for (auto it = polls.begin(); it != polls.end();) {
        if (uint32_t(millis() - it->command.started) >= ACK_TIMEOUT) {
            if (!it->command.positionReceived && !it->command.telemetryReceived && !it->command.nodeInfoReceived)
                finishDiagnostic(it->command, false);
            it = polls.erase(it); continue;
        }
        if (it->sent == it->count && it->replies == (1U << it->count) - 1) {
            it = polls.erase(it); continue;
        }
        if (moduleConfig.mqtt.enabled && !authBlocked && !inputFault && authenticated && socketConnected && nowMs() && it->sent < it->count && int32_t(millis() - it->nextAt) >= 0) {
            const unsigned step = it->needsInfo ? it->sent : it->sent + 1;
            auto *p = packet(it->command.target);
            if (!p) { finishDiagnostic(it->command, it->replies != 0); it = polls.erase(it); continue; }
            p->decoded.want_response = true;
            if (step == 0) {
                p->decoded.portnum = meshtastic_PortNum_NODEINFO_APP;
                p->decoded.payload.size = pb_encode_to_bytes(p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes), &meshtastic_User_msg, &owner);
            }
            else if (step == 1) p->decoded.portnum = meshtastic_PortNum_POSITION_APP;
            else {
                meshtastic_Telemetry t = meshtastic_Telemetry_init_default;
                t.which_variant = meshtastic_Telemetry_device_metrics_tag;
                t.time = uint32_t(nowMs()/1000);
                p->decoded.portnum = meshtastic_PortNum_TELEMETRY_APP;
                p->decoded.payload.size = pb_encode_to_bytes(p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes), &meshtastic_Telemetry_msg, &t);
            }
            uint32_t id = p->id;
            if (!transmit(p)) { finishDiagnostic(it->command, it->replies != 0); it = polls.erase(it); continue; }
            LOG_INFO("MVT poll target=%08x port=%u id=%u", it->command.target, unsigned(step == 0 ? 4 : step == 1 ? 3 : 67), id);
            it->ids[it->sent++] = id;
            it->nextAt = millis() + it->spacing;
        }
        ++it;
    }
}
int32_t MvtWssGateway::runOnce() {
    std::string room = moduleConfig.mqtt.username, secret = moduleConfig.mqtt.password, addr = moduleConfig.mqtt.address;
    if (room != configuredRoom || secret != configuredKey || addr != configuredAddress) {
        disconnectWs(); authBlocked = false; displayError = nullptr;
        outgoing.clear(); pending.clear(); polls.clear(); completedCommands.clear();
        configuredRoom = room; configuredKey = secret; configuredAddress = addr;
    }
    for (auto it = pending.begin(); it != pending.end();) {
        if (uint32_t(millis() - it->started) >= ACK_TIMEOUT) {
            if (it->kind == "chat") {
                status(it->id, "UNCONFIRMED"); it->kind = "latechat"; ++it;
            } else if (it->kind == "latechat" && uint32_t(millis() - it->started) < 600000) ++it;
            else {
                if (it->kind != "latechat") finishDiagnostic(*it, false);
                it = pending.erase(it);
            }
        } else ++it;
    }
    // Keep a bounded late-ACK window without exhausting active command slots.
    while (pending.size() > MAX_PENDING + 64) {
        auto oldest = std::find_if(pending.begin(), pending.end(), [](const Pending &p) { return p.kind == "latechat"; });
        if (oldest == pending.end()) break;
        pending.erase(oldest);
    }
    // A server ERROR can arrive immediately before a close event. Process it
    // before reconnect logic, so replacement/admin rejection cannot loop.
    char *text = nullptr;
    for (int i = 0; i < 8 && incoming && xQueueReceive(incoming, &text, 0) == pdTRUE; ++i) {
        handleIncomingCommand(text); free(text);
    }
    advancePolls();
    if (peerRejected) { authBlocked = true; if (!displayError) displayError = "BLOCK"; peerRejected = false; }
    if (!moduleConfig.mqtt.enabled || room.empty() || secret.empty() || WiFi.status() != WL_CONNECTED || authBlocked || inputFault) {
        if (wsClient) disconnectWs();
        if (!moduleConfig.mqtt.enabled) { authBlocked = false; displayError = nullptr; }
        return 1000;
    }
    if (!socketConnected) {
        authenticated = false; authSent = false;
        if (uint32_t(millis() - lastConnectAttemptMs) > 15000) { lastConnectAttemptMs = millis(); connectWs(); }
        return 100;
    }
    if (!authSent) sendAuthGateway();
    if (!authenticated) {
        if (authSent && uint32_t(millis() - authStartedMs) > 10000) disconnectWs();
        return 100;
    }
    if (uint32_t(millis() - lastHeartbeatMs) >= 15000) sendHeartbeat();
    // Wait for a clock before replaying events; never send uptime as Unix time.
    if (!nowMs()) return 100;
    advancePolls();
    if (snapshotPending) sendSnapshot();
    for (int i = 0; i < 4 && !outgoing.empty(); ++i) {
        auto p = json(cJSON_Parse(outgoing.front().c_str()));
        // Events captured before time sync get receive time, not 1970.
        cJSON *m = cJSON_GetObjectItemCaseSensitive(p.get(), "message");
        if (m && number(m, "timestampMs") < 1577836800000.0) setNumber(m, "timestampMs", double(nowMs()));
        if (!sendDocument(p.get())) break;
        outgoing.pop_front();
    }
    return 100;
}
#endif
