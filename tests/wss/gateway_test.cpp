#include <pb_decode.h>
// Compile the production gateway with deterministic radio/socket adapters.
#include "configuration.h"
#include <memory>
#include <map>
#include <cassert>
#include <fstream>
#include <sstream>
#include "NodeDB.h"
#include "Router.h"
#include "RadioLibInterface.h"
RadioLibInterface testRadioInfo;
RadioLibInterface *RadioLibInterface::instance = &testRadioInfo;
#include "PowerStatus.h"
#include "WiFi.h"
#include "esp_websocket_client.h"
#include "concurrency/OSThread.h"
#include <cJSON.h>
#define private public
#define protected public
#include "modules/MvtWssGateway.h"
#undef protected
#undef private
#include "modules/MvtPayload.h"
#include "graphics/MvtCompactScreen.h"
uint32_t testMillis = 20000;
meshtastic_LocalConfig config = meshtastic_LocalConfig_init_default;
meshtastic_LocalModuleConfig moduleConfig = meshtastic_LocalModuleConfig_init_default;
meshtastic_User owner = meshtastic_User_init_default;
Pool packetPool;
Channels channels;
NodeDB db;
NodeDB *nodeDB = &db;
Router radio;
Router *router = &radio;
PowerStatus power;
PowerStatus *powerStatus = &power;
TestWiFi WiFi;
std::vector<std::string> testSent;
bool testSendFailure = false;
bool testSendBusy = false;
bool testOtaWorkerRunning = false;
// Flash NodeDB serialization is outside this host adapter; wire protobufs
// still use the actual generated 2.7.15 descriptors and nanopb implementation.
bool meshtastic_NodeDatabase_callback(pb_istream_t *, pb_ostream_t *, const pb_field_t *) { return false; }
int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while(0)
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json parse(const std::string &s) { return Json(cJSON_Parse(s.c_str()), cJSON_Delete); }
const char *sget(cJSON *j, const char *key) { auto *v = cJSON_GetObjectItemCaseSensitive(j, key); return cJSON_IsString(v) ? v->valuestring : ""; }
double nget(cJSON *j, const char *key) { auto *v = cJSON_GetObjectItemCaseSensitive(j, key); return cJSON_IsNumber(v) ? v->valuedouble : 0; }
void frame(MvtWssGateway &g, const std::string &s, size_t chunk = 2048) {
    for (size_t off = 0; off < s.size(); off += chunk) {
        esp_websocket_event_data_t d = {s.data() + off, int(std::min(chunk, s.size() - off)), 1, nullptr, nullptr, int(s.size()), int(off)};
        g.receiveFrame(d);
    }
}
void flush(MvtWssGateway &g) {
    for (int i = 0; i < 100 && (!g.outgoing.empty() || g.snapshotPending); ++i) g.runOnce();
}
Json lastEvent(MvtWssGateway &g, const char *type) {
    flush(g);
    for (auto it = testSent.rbegin(); it != testSent.rend(); ++it) {
        auto j = parse(*it); if (strcmp(sget(j.get(), "type"), type) == 0) return j;
    }
    return Json(nullptr, cJSON_Delete);
}
meshtastic_MeshPacket incoming(uint32_t from, uint32_t req, meshtastic_PortNum port) {
    meshtastic_MeshPacket p = meshtastic_MeshPacket_init_default;
    p.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    p.from = from; p.to = db.getNodeNum(); p.id = radio.nextId++; p.decoded.request_id = req;
    p.decoded.portnum = port; p.hop_start = 3; p.hop_limit = 1; p.rx_snr = 6.25; return p;
}
void ack(MvtWssGateway &g, uint32_t from, uint32_t id, int reason = 0, uint32_t to = 0x11223344) {
    auto p = incoming(from, id, meshtastic_PortNum_ROUTING_APP); p.to = to;
    meshtastic_Routing r = meshtastic_Routing_init_default;
    r.which_variant = meshtastic_Routing_error_reason_tag; r.error_reason = meshtastic_Routing_Error(reason);
    p.decoded.payload.size = pb_encode_to_bytes(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes), &meshtastic_Routing_msg, &r);
    g.handleReceived(p);
}
void command(MvtWssGateway &g, const char *text) {
    g.handleIncomingCommand(text);
    // Existing wire assertions inspect the next actual dispatch, after spacing.
    bool queued=std::any_of(g.pending.begin(),g.pending.end(),[](const MvtWssGateway::Pending &p){return !p.id;}) ||
        std::any_of(g.polls.begin(),g.polls.end(),[](const MvtWssGateway::Poll &p){return !p.sent;}) ||
        std::any_of(g.airPolls.begin(),g.airPolls.end(),[](const MvtWssGateway::AirPoll &p){return !p.sent;});
    if(queued&&!g.diagnosticSlot()){testMillis=g.nextDiagnosticAt;g.runOnce();}
}
int main(int argc, char **argv) {
    config.lora.tx_enabled = true; config.lora.hop_limit = 4; config.lora.use_preset=true; config.lora.modem_preset=meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST; moduleConfig.mqtt.enabled = true;
    strcpy(moduleConfig.mqtt.username, "ROOM1"); strcpy(moduleConfig.mqtt.password, "relay-secret");
    strcpy(owner.short_name, "MVT"); strcpy(owner.long_name, "Movetain Host");
    meshtastic_NodeInfoLite peer = {}; peer.num = 0xaabbccdd; peer.channel = 2;
    peer.last_heard = 1700000000; peer.has_user = true; strcpy(peer.user.short_name, "A"); db.nodes.push_back(peer);
    MvtWssGateway g;
    testOtaWorkerRunning=true;g.runOnce();CHECK(g.wsClient==nullptr);
    testOtaWorkerRunning=false;
    g.runOnce(); CHECK(g.wsClient != nullptr); CHECK(!g.authenticated);
    g.socketConnected = true; g.runOnce(); CHECK(g.authSent);
    CHECK(std::string(sget(parse(testSent.back()).get(), "type")) == "AUTH_GATEWAY");
    command(g, "{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":42,\"text\":\"blocked\"}"); CHECK(radio.sent.empty());
    frame(g, "{\"type\":\"AUTH_GATEWAY_SUCCESS\"}", 5); g.runOnce(); CHECK(g.authenticated);
    frame(g, "{\"type\":\"GATEWAY_HEARTBEAT_ACK\",\"timestamp\":1791892800000}"); g.runOnce();
    CHECK(g.nowMs() == 1791892800000ULL); flush(g);
    {
        auto e=parse("{\"type\":\"TEST_CONTENTION\"}");
        testSendBusy=true;CHECK(!g.sendDocument(e.get()));CHECK(!g.inputFault);CHECK(g.socketConnected);
        testSendBusy=false;CHECK(g.sendDocument(e.get()));
        testSendFailure=true;CHECK(!g.sendDocument(e.get()));CHECK(g.inputFault);
        testSendFailure=false;g.inputFault=false;
        testSent.pop_back();
    }
    auto initialNode = lastEvent(g, "NODE_UPDATE");
    CHECK(nget(cJSON_GetObjectItem(initialNode.get(), "node"), "lastUpdateMs") == 1700000000000ULL);
    auto gateway = Json(g.gatewayInfo(), cJSON_Delete);
    CHECK(std::string(sget(gateway.get(), "nodeName")) == "Movetain Host");
    CHECK(std::string(sget(gateway.get(), "shortName")) == "MVT");
    CHECK(std::string(sget(gateway.get(), "frequency")) == "433.175 MHz");
    meshtastic_NodeInfoLite unknown = {}; unknown.num = 0x55555555;
    meshtastic_NodeInfoLite ignored = {}; ignored.num = 0x66666666; ignored.is_ignored = true;
    meshtastic_NodeInfoLite own = {}; own.num = db.getNodeNum();
    db.nodes.push_back(unknown); db.nodes.push_back(ignored); db.nodes.push_back(own);
    const size_t snapshotStart = testSent.size(); g.snapshotPending = true; flush(g);
    unsigned snapshotNodes = 0;
    for (size_t i = snapshotStart; i < testSent.size(); ++i) {
        auto item = parse(testSent[i]);
        if (std::string(sget(item.get(), "type")) != "NODE_UPDATE") continue;
        ++snapshotNodes; auto *data = cJSON_GetObjectItem(item.get(), "node");
        CHECK(nget(data,"nodeId") != ignored.num && nget(data,"nodeId") != own.num);
        if (nget(data,"nodeId") == unknown.num) CHECK(nget(data,"lastUpdateMs") == 0);
    }
    CHECK(snapshotNodes == 2);
    // JSON escaping, signed Android node ID, exact ID and channel, binary MV.
    command(g, u8"{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":42,\"recipientId\":-1430532899,\"senderCallsign\":\"HQ\",\"text\":\"Привет \\\"А\\\"\\nстрока\",\"isSos\":true}");
    CHECK(radio.sent.size() == 1); auto sent = radio.sent.back();
    CHECK(sent.to == 0xaabbccdd && sent.id == 42 && sent.want_ack && sent.channel == 2);
    CHECK(sent.decoded.portnum == meshtastic_PortNum_PRIVATE_APP);
    mvt::Payload payload; CHECK(mvt::decode(sent.decoded.payload.bytes, sent.decoded.payload.size, payload));
    CHECK(payload.comment == u8"Привет \"А\"\nстрока" && payload.flags == 6);
    command(g, "{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":42,\"text\":\"duplicate\"}"); CHECK(radio.sent.size() == 1);
    ack(g, 9, 42); ack(g, db.getNodeNum(), 42); ack(g, 0xaabbccdd, 42, 0, 5); ack(g, 0xaabbccdd, 43);
    CHECK(g.pending.size() == 1);
    ack(g, 0xaabbccdd, 42); CHECK(g.pending.empty());
    auto st = lastEvent(g, "CHAT_STATUS_UPDATE"); CHECK(std::string(sget(st.get(), "status")) == "DELIVERED");
    command(g, "{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":44,\"text\":\"broadcast\"}");
    CHECK(radio.sent.back().to == 0xffffffff && !radio.sent.back().want_ack);
    CHECK(std::string(sget(lastEvent(g, "CHAT_STATUS_UPDATE").get(), "status")) == "SENT");
    size_t count = radio.sent.size();
    command(g, "{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":45,\"text\":\"bad\"}garbage"); CHECK(radio.sent.size() == count);
    command(g, "{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":45,\"recipientId\":2.5,\"text\":\"bad\"}"); CHECK(radio.sent.size() == count);
    std::string longText(234, 'x');
    command(g, ("{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":46,\"text\":\"" + longText + "\"}").c_str());
    CHECK(radio.sent.size() == count); CHECK(std::string(sget(lastEvent(g, "CHAT_STATUS_UPDATE").get(), "status")) == "FAILED");
    // Timeout is unconfirmed; a late target ACK can still prove delivery.
    command(g, "{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":47,\"recipientId\":2864434397,\"text\":\"late\"}");
    testMillis += 120001; g.runOnce();
    CHECK(std::string(sget(lastEvent(g, "CHAT_STATUS_UPDATE").get(), "status")) == "UNCONFIRMED");
    ack(g, 0xaabbccdd, 47); CHECK(std::string(sget(lastEvent(g, "CHAT_STATUS_UPDATE").get(), "status")) == "DELIVERED");
    // Dispatch failure is not delivery.
    radio.fail = true; command(g, "{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":48,\"recipientId\":2,\"text\":\"fail\"}"); radio.fail = false;
    CHECK(std::string(sget(lastEvent(g, "CHAT_STATUS_UPDATE").get(), "status")) == "FAILED");
    command(g, "{\"type\":\"CMD_TX_LORA_WAYPOINT\",\"recipientId\":2864434397,\"waypoint\":{\"title\":\"Target\",\"comment\":\"точка\",\"latitude\":43.2,\"longitude\":76.8,\"altitude\":1234,\"isTarget\":true}}");
    CHECK(radio.sent.back().to == 0xaabbccdd); CHECK(mvt::decode(radio.sent.back().decoded.payload.bytes, radio.sent.back().decoded.payload.size, payload));
    CHECK(payload.icon == 5 && payload.altitude == 1234 && payload.name == "Target");
    // Known node: position and telemetry, independently correlated and paced.
    command(g, "{\"type\":\"CMD_PING_NODE\",\"targetNodeId\":2864434397}"); sent = radio.sent.back();
    CHECK(sent.decoded.want_response && sent.decoded.portnum == meshtastic_PortNum_POSITION_APP);
    const uint32_t positionId = sent.id;
    const size_t beforeThrottle = radio.sent.size();
    command(g, "{\"type\":\"CMD_PING_NODE\",\"targetNodeId\":2864434397}");
    CHECK(radio.sent.size() == beforeThrottle);
    testMillis += g.polls.front().spacing; g.runOnce(); sent = radio.sent.back();
    CHECK(sent.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP && sent.id != positionId);
    meshtastic_Telemetry t = meshtastic_Telemetry_init_default; t.which_variant = meshtastic_Telemetry_device_metrics_tag;
    t.variant.device_metrics.has_battery_level = true; t.variant.device_metrics.battery_level = 66;
    auto p = incoming(0xaabbccdd, sent.id + 50, meshtastic_PortNum_TELEMETRY_APP);
    p.decoded.payload.size = pb_encode_to_bytes(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes), &meshtastic_Telemetry_msg, &t);
    g.handleReceived(p); CHECK(!g.polls.front().command.telemetryReceived);
    p.decoded.request_id = sent.id; p.id = radio.nextId++; g.handleReceived(p);
    CHECK(g.polls.front().command.telemetryReceived && !g.polls.front().command.positionReceived);
    auto result = lastEvent(g, "PING_RESULT"); CHECK(nget(result.get(), "batteryPct") == 66 && nget(result.get(), "hops") == 2);
    // A delayed position reply remains valid after telemetry has already arrived.
    meshtastic_Position position = meshtastic_Position_init_default;
    position.has_latitude_i = true; position.has_longitude_i = true;
    position.latitude_i = 499400000; position.longitude_i = 826100000;
    p = incoming(0xaabbccdd, positionId, meshtastic_PortNum_POSITION_APP);
    p.decoded.payload.size = pb_encode_to_bytes(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes), &meshtastic_Position_msg, &position);
    g.handleReceived(p); g.runOnce(); CHECK(g.polls.empty());
    auto node = lastEvent(g, "NODE_UPDATE");
    CHECK(nget(cJSON_GetObjectItem(node.get(), "node"), "lat") == 49.94);
    // Empty GPS reply does not delete the last known location.
    p = incoming(0xaabbccdd, 0, meshtastic_PortNum_POSITION_APP);
    position = meshtastic_Position_init_default;
    p.decoded.payload.size = pb_encode_to_bytes(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes), &meshtastic_Position_msg, &position);
    g.handleReceived(p); node = lastEvent(g, "NODE_UPDATE");
    CHECK(nget(cJSON_GetObjectItem(node.get(), "node"), "lat") == 49.94);
    CHECK(cJSON_IsFalse(cJSON_GetObjectItem(cJSON_GetObjectItem(node.get(), "node"), "gpsFix")));
    // Unknown node gets three requests with independent IDs.
    command(g, "{\"type\":\"CMD_PING_NODE\",\"targetNodeId\":1234567}");
    CHECK(radio.sent.back().decoded.portnum == meshtastic_PortNum_NODEINFO_APP);
    testMillis += g.polls.front().spacing; g.runOnce();
    CHECK(radio.sent.back().decoded.portnum == meshtastic_PortNum_POSITION_APP);
    testMillis += g.polls.front().spacing; g.runOnce();
    CHECK(radio.sent.back().decoded.portnum == meshtastic_PortNum_TELEMETRY_APP);
    testMillis += 120001; g.runOnce(); CHECK(g.polls.empty());
    command(g, "{\"type\":\"CMD_POLL_ENV\",\"targetNodeId\":2864434397}"); sent = radio.sent.back();
    t = meshtastic_Telemetry_init_default; t.which_variant = meshtastic_Telemetry_environment_metrics_tag;
    t.variant.environment_metrics.has_temperature = true; t.variant.environment_metrics.temperature = -12.5;
    t.variant.environment_metrics.has_gas_resistance = true; t.variant.environment_metrics.gas_resistance = 125.5;
    t.variant.environment_metrics.has_relative_humidity = true; t.variant.environment_metrics.relative_humidity = 75;
    p = incoming(0xaabbccdd, sent.id, meshtastic_PortNum_TELEMETRY_APP);
    p.decoded.payload.size = pb_encode_to_bytes(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes), &meshtastic_Telemetry_msg, &t);
    g.handleReceived(p); result = lastEvent(g, "POLL_ENV_RESULT"); CHECK(nget(result.get(), "temperature") == -12.5);
    CHECK(nget(result.get(), "gasResistance") == 125.5);
    // Real AirQualityMetrics wire fields: explicit zero wins over environmental fallback.
    t = meshtastic_Telemetry_init_default; t.which_variant = meshtastic_Telemetry_air_quality_metrics_tag;
    t.variant.air_quality_metrics.has_pm25_standard = true; t.variant.air_quality_metrics.pm25_standard = 0;
    t.variant.air_quality_metrics.has_pm25_environmental = true; t.variant.air_quality_metrics.pm25_environmental = 19;
    t.variant.air_quality_metrics.has_co2 = true; t.variant.air_quality_metrics.co2 = 720;
    p = incoming(0xaabbccdd, 0, meshtastic_PortNum_TELEMETRY_APP);
    p.decoded.payload.size = pb_encode_to_bytes(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes), &meshtastic_Telemetry_msg, &t);
    g.handleReceived(p); result = lastEvent(g, "NODE_UPDATE");
    auto *airNode = cJSON_GetObjectItem(result.get(), "node");
    CHECK(nget(airNode, "pm25") == 0 && nget(airNode, "co2") == 720);
    CHECK(!cJSON_GetObjectItem(airNode, "iaq"));
    command(g, "{\"type\":\"CMD_TRACEROUTE\",\"targetNodeId\":2864434397}"); sent = radio.sent.back();
    meshtastic_RouteDiscovery route = meshtastic_RouteDiscovery_init_default;
    route.route_count = 1; route.route[0] = 7; route.snr_towards_count = 2; route.snr_towards[0] = 20; route.snr_towards[1] = -8;
    route.route_back_count = 1; route.route_back[0] = 8; route.snr_back_count = 2; route.snr_back[0] = 12; route.snr_back[1] = 24;
    p = incoming(0xaabbccdd, sent.id, meshtastic_PortNum_TRACEROUTE_APP);
    p.decoded.payload.size = pb_encode_to_bytes(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes), &meshtastic_RouteDiscovery_msg, &route);
    g.handleReceived(p); result = lastEvent(g, "TRACEROUTE_RESULT");
    CHECK(cJSON_GetArraySize(cJSON_GetObjectItem(result.get(), "outbound")) == 1); CHECK(nget(result.get(), "outboundLastSnr") == -2);
    // Virtual beacon, optional signed altitude/heart rate, JSON escaping.
    payload = mvt::Payload(); payload.name = u8"А\"Б"; payload.comment = u8"маяк"; payload.lat = 43.25; payload.lon = 76.9;
    payload.flags = 16 | 32; payload.altitude = -20; payload.heartRate = 101; payload.battery = 80; payload.charging = true;
    p = incoming(0xaabbccdd, 0, meshtastic_PortNum_PRIVATE_APP);
    p.decoded.payload.size = mvt::encode(payload, p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes));
    g.handleReceived(p); result = lastEvent(g, "NODE_UPDATE"); auto *n = cJSON_GetObjectItem(result.get(), "node");
    CHECK(std::string(sget(n, "nodeKey")) == "!aabbccdd_mv"); CHECK(nget(n, "alt") == -20 && nget(n, "heartRate") == 101);
    CHECK(nget(n, "lastUpdateMs") >= 1577836800000.0);
    size_t events = testSent.size(); g.handleReceived(p); flush(g); CHECK(testSent.size() == events);
    // Offline reception is cached and replayed after a successful auth.
    g.authenticated = false; p.id++; payload.name = "Offline";
    p.decoded.payload.size = mvt::encode(payload, p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes)); g.handleReceived(p);
    CHECK(g.nodes.count("!aabbccdd_mv") == 1); command(g, "{\"type\":\"AUTH_GATEWAY_SUCCESS\"}"); flush(g);
    result = lastEvent(g, "NODE_UPDATE"); CHECK(std::string(sget(cJSON_GetObjectItem(result.get(), "node"), "callsign")) == "Offline");
    // Waypoint deletion removes local replay cache, so it cannot resurrect.
    payload.flags = 1; payload.name = "Target"; p.id++;
    p.decoded.payload.size = mvt::encode(payload, p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes)); g.handleReceived(p); CHECK(g.waypoints.size() == 1);
    command(g, "{\"type\":\"WAYPOINT_DELETED\",\"latitude\":43.25,\"longitude\":76.9}"); CHECK(g.waypoints.empty());
    for (const auto &queued : g.outgoing) {
        auto e = parse(queued);
        CHECK(std::string(sget(e.get(), "type")) != "WAYPOINT");
        auto *msg = cJSON_GetObjectItemCaseSensitive(e.get(), "message");
        if (msg) CHECK(!cJSON_GetObjectItemCaseSensitive(msg, "waypoint"));
    }
    // Actual MV radio packets: sender identity is not the waypoint title.
    p.from = 0xddee1122;
    payload.flags = 1; payload.name = "Field Sender"; payload.comment = "Fast Location"; p.id++;
    p.decoded.payload.size = mvt::encode(payload, p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes)); g.handleReceived(p); flush(g);
    result = lastEvent(g, "WAYPOINT"); auto *wp = cJSON_GetObjectItem(result.get(), "waypoint");
    CHECK(std::string(sget(wp, "title")) == "Fast Location"); CHECK(nget(wp, "latitude") == 43.25);
    // SOS coordinates form a point even when the waypoint flag is absent.
    payload.flags = 4; payload.comment = "SOS"; p.id++;
    p.decoded.payload.size = mvt::encode(payload, p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes)); g.handleReceived(p); flush(g);
    result = lastEvent(g, "WAYPOINT"); wp = cJSON_GetObjectItem(result.get(), "waypoint");
    CHECK(std::string(sget(wp, "title")) == "SOS Emergency"); CHECK(cJSON_IsTrue(cJSON_GetObjectItem(wp, "isSos")));
    result = lastEvent(g, "CHAT_MESSAGE");
    CHECK(cJSON_GetObjectItem(cJSON_GetObjectItem(result.get(), "message"), "waypoint") != nullptr);
    CHECK(g.waypoints.size() == 2);
    command(g, "{\"type\":\"WAYPOINT_DELETED\",\"latitude\":43.25,\"longitude\":76.9,\"title\":\"Fast Location\"}");
    CHECK(g.waypoints.size() == 1); CHECK(g.waypoints.begin()->second.find("SOS Emergency") != std::string::npos);
    // Packet fragments assemble before dispatch; control frames do not pollute JSON.
    // One persisted radio setting applies to portal and normal device packets.
    CHECK(g.effectiveHopLimit() == config.lora.hop_limit);
    auto *hopPacket = g.packet(0xaabbccdd); CHECK(hopPacket->hop_limit == 4); packetPool.release(hopPacket);
    command(g, "{\"type\":\"CMD_SET_WSS_HOP_LIMIT\",\"hopLimit\":2}");
    hopPacket = g.packet(0xaabbccdd); CHECK(hopPacket->hop_limit == 2 && hopPacket->hop_start == 2); packetPool.release(hopPacket);
    CHECK(config.lora.hop_limit == 2); CHECK(db.configSaves == 1);
    for (const char *invalid : {"0", "8", "1.5", "\"3\""}) {
        std::string update = "{\"type\":\"CMD_SET_WSS_HOP_LIMIT\",\"hopLimit\":" + std::string(invalid) + "}";
        command(g, update.c_str()); CHECK(config.lora.hop_limit == 2);
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> hopInfo(g.gatewayInfo(), cJSON_Delete); CHECK(nget(hopInfo.get(), "hopLimit") == 2);
    CHECK(nget(hopInfo.get(), "radioHopLimit") == 2); CHECK(cJSON_IsTrue(cJSON_GetObjectItem(hopInfo.get(), "hopLimitControl")));
    command(g, "{\"type\":\"AUTH_GATEWAY_SUCCESS\",\"wssHopLimit\":5}"); CHECK(config.lora.hop_limit == 2);
    command(g, "{\"type\":\"AUTH_GATEWAY_SUCCESS\"}"); CHECK(g.effectiveHopLimit() == config.lora.hop_limit);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(hopInfo.get(), "unifiedHopLimit")));
    command(g, "{\"type\":\"CMD_SET_WSS_HOP_LIMIT\",\"hopLimit\":2}"); CHECK(db.configSaves == 1);
    db.saveSucceeds = false;
    command(g, "{\"type\":\"CMD_SET_WSS_HOP_LIMIT\",\"hopLimit\":7}"); CHECK(config.lora.hop_limit == 2);
    db.saveSucceeds = true;
    config.lora.hop_limit = 6; // Meshtastic admin-side change, followed by reconnect.
    command(g, "{\"type\":\"AUTH_GATEWAY_SUCCESS\",\"wssHopLimit\":3}"); CHECK(g.effectiveHopLimit() == 6);
    hopPacket = g.packet(0xaabbccdd); CHECK(hopPacket->hop_limit == 6); packetPool.release(hopPacket);
    std::string padded = "{\"type\":\"CMD_TX_LORA_CHAT\",\"id\":99,\"text\":\"chunked\",\"padding\":\"" + std::string(5000, 'a') + "\"}";
    count = radio.sent.size(); frame(g, padded, 111); CHECK(radio.sent.size() == count); g.runOnce(); CHECK(radio.sent.size() == count + 1);
    esp_websocket_event_data_t d = {"ping", 4, 9, nullptr, nullptr, 4, 0}; g.receiveFrame(d); CHECK(!g.inputFault);
    d = {"bad", 3, 1, nullptr, nullptr, 3, 1}; g.receiveFrame(d); CHECK(g.inputFault);
    g.inputFault = false;
    frame(g, "{\"type\":\"ERROR\",\"code\":\"GATEWAY_REPLACED\"}");
    g.socketConnected = false; g.runOnce(); CHECK(g.authBlocked); CHECK(!g.socketConnected);
    CHECK(std::string(g.displayStatus()) == "TAKEN");
    command(g, "{\"type\":\"ERROR\",\"code\":\"INVALID_RELAY_KEY\"}"); CHECK(std::string(g.displayStatus()) == "BADKEY");
    command(g, "{\"type\":\"ERROR\",\"code\":\"ROOM_NOT_FOUND\"}"); CHECK(std::string(g.displayStatus()) == "NOROOM");
    moduleConfig.mqtt.enabled = false; CHECK(std::string(g.displayStatus()) == "OFF"); moduleConfig.mqtt.enabled = true;
    g.displayError = nullptr; g.authenticated = false; g.socketConnected = true;
    CHECK(std::string(g.displayStatus()) == "AUTH");
    g.authenticated = true; CHECK(std::string(g.displayStatus()) == "OK");
    g.socketConnected = false; CHECK(std::string(g.displayStatus()) == "RETRY");
    g.inputFault = true; CHECK(std::string(g.displayStatus()) == "FRAME"); g.inputFault = false;
    for (const char *status : {"BADKEY","NOROOM","TAKEN","BLOCK","BADURL","FAIL","FRAME","SETUP","SETKEY","WAIT","RETRY","AUTH","OK","OFF"})
        CHECK(mvtScreen::statusLine(status).size() <= 10);
    CHECK(mvtScreen::roomLine("ABC123", 0) == "R:ABC123");
    const std::string longRoom = "ROOM_1234567890";
    std::string visible;
    for (unsigned i=0;i<longRoom.size()+3;++i) {
        auto line = mvtScreen::roomLine(longRoom.c_str(), i*700);
        CHECK(line.size() == 10); visible += line.substr(2,1);
    }
    CHECK(visible == longRoom + "   ");
    for (uint32_t value : {0U,999U,1000U,99999U,100000U,1000000U,UINT32_MAX}) {
        CHECK(mvtScreen::count(value).size() <= 3);
        CHECK(("R:"+mvtScreen::count(value)+"T:"+mvtScreen::count(value)).size() <= 10);
    }
    struct Canvas {
        uint8_t pixels[64*32] = {};
        void setPixel(int x,int y) { if (!(x>=0 && x<64 && y>=0 && y<32)) { CHECK(false); } pixels[y*64+x]=255; }
    };
    Canvas splash;
    mvtScreen::logo(&splash,0,0);
    CHECK(mvtScreen::networkName("ShortSSID",0)=="ShortSSID");
    CHECK(mvtScreen::networkName("Galaxy S21 Ultra 5G",0)=="Galaxy S21");
    CHECK(mvtScreen::networkName("Galaxy S21 Ultra 5G",700)=="alaxy S21 ");
    for (const char *preset : {"LONG_FAST", "LONG_SLOW", "VERY_LONG_SLOW", "MEDIUM_SLOW", "MEDIUM_FAST", "SHORT_SLOW", "SHORT_FAST", "LONG_MODERATE", "SHORT_TURBO", "CUSTOM"})
        CHECK(mvtScreen::presetLine(preset).size() <= 10);
    CHECK(mvtScreen::frequencyLine(439.225) == "439.225MHZ");
    CHECK(mvtScreen::frequencyLine(2400.123) == "2400.12MHZ");
    CHECK(mvtScreen::frequencyLine(0) == "FREQ:--");
    CHECK(g.effectiveHopLimit() == config.lora.hop_limit);
    CHECK(mvtScreen::memoryLine("FREE", 168*1024) == "FREE:168K");
    CHECK(mvtScreen::memoryLine("MIN", 112*1024+1023) == "MIN:112K");
    CHECK(mvtScreen::memoryLine("BLOCK", 0) == "BLOCK:0K");
    CHECK(mvtScreen::memoryLine("BLOCK", 512*1024).size() <= 10);
    CHECK(!mvtScreen::alternatePhase(4));
    CHECK(mvtScreen::alternatePhase(5));
    CHECK(mvtScreen::alternatePhase(9));
    CHECK(!mvtScreen::alternatePhase(10));
    CHECK(mvtScreen::voltageLine(3929) == "VLT:3.92V");
    CHECK(mvtScreen::voltageLine(-1) == "VLT:--");
    CHECK(mvtScreen::voltageLine(0) == "VLT:--");
    CHECK(mvtScreen::uptimeLine(0) == "00:00");
    CHECK(mvtScreen::uptimeLine(86399) == "23:59");
    CHECK(mvtScreen::uptimeLine(184440) == "2D 03:14");
    CHECK(mvtScreen::uptimeLine(4294967) == "49D 17:02");
    if (argc > 3) {
        Canvas memoryCanvas;
        mvtScreen::row(&memoryCanvas,0,1,mvtScreen::memoryLine("FREE",168*1024));
        mvtScreen::row(&memoryCanvas,0,11,mvtScreen::memoryLine("MIN",112*1024));
        mvtScreen::row(&memoryCanvas,0,21,mvtScreen::memoryLine("BLOCK",96*1024));
        std::ofstream memoryImage(std::string(argv[3])+"/screen-memory.pgm",std::ios::binary);
        memoryImage << "P5\n64 32\n255\n"; memoryImage.write(reinterpret_cast<char *>(memoryCanvas.pixels),sizeof(memoryCanvas.pixels));
        Canvas radioCanvas;
        mvtScreen::row(&radioCanvas,0,1,mvtScreen::presetLine("LONG_FAST"));
        mvtScreen::row(&radioCanvas,0,11,mvtScreen::frequencyLine(439.225));
        mvtScreen::row(&radioCanvas,0,21,"HOPS:3");
        std::ofstream radioImage(std::string(argv[3])+"/screen-radio.pgm",std::ios::binary);
        radioImage << "P5\n64 32\n255\n"; radioImage.write(reinterpret_cast<char *>(radioCanvas.pixels),sizeof(radioCanvas.pixels));
        const char *names[] = {"online", "badkey", "stats", "long-room"};
        const char *lines[][3] = {{"R:ABC123","WIFI:OK","WSS:OK"},{"R:ABC123","WIFI:OK","WSS:BADKEY"},
                                  {"BAT:85%","N:12","R:123T:456"},{"R:ROOM_123","WIFI:OFF","WSS:WAIT"}};
        for (unsigned i=0;i<4;++i) {
            Canvas canvas;
            for(int row=0;row<3;++row) mvtScreen::row(&canvas,0,1+row*10,lines[i][row]);
            std::ofstream image(std::string(argv[3])+"/screen-"+names[i]+".pgm",std::ios::binary);
            image << "P5\n64 32\n255\n"; image.write(reinterpret_cast<char *>(canvas.pixels),sizeof(canvas.pixels));
        }
        for (unsigned i=0;i<2;++i) {
            Canvas canvas;
            if (i==0) {
                mvtScreen::row(&canvas,0,1,mvtScreen::networkName("Galaxy S21 Ultra 5G",0));
                mvtScreen::row(&canvas,0,11,"IP:192.168");
                mvtScreen::row(&canvas,0,21,"   .1.100");
            } else {
                mvtScreen::clockLogo(&canvas,2,1);
                mvtScreen::row(&canvas,32,5,"20:35");
                mvtScreen::row(&canvas,0,23,"13.10.2026");
            }
            std::ofstream image(std::string(argv[3])+(i==0?"/screen-network.pgm":"/screen-clock.pgm"),std::ios::binary);
            image << "P5\n64 32\n255\n"; image.write(reinterpret_cast<char *>(canvas.pixels),sizeof(canvas.pixels));
        }
    }
    // Adaptive per-target windows and a shared radio dispatch slot.
    {
        auto previous=config.lora;
        meshtastic_NodeInfoLite near={};near.num=0x99887766;near.has_hops_away=true;near.hops_away=0;
        near.has_user=true;strcpy(near.user.long_name,"Near");db.nodes.push_back(near);
        config.lora.use_preset=true;config.lora.modem_preset=meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;
        MvtWssGateway adaptive;adaptive.authenticated=true;adaptive.socketConnected=true;adaptive.authSent=true;
        adaptive.wsClient=esp_websocket_client_init(nullptr);adaptive.serverTimeMs=1700000000000ULL;adaptive.serverTimeTick=testMillis;
        adaptive.configuredRoom=moduleConfig.mqtt.username;adaptive.configuredKey=moduleConfig.mqtt.password;adaptive.configuredAddress=moduleConfig.mqtt.address;
        CHECK(adaptive.diagnosticStep(near.num)==3000);
        CHECK(adaptive.diagnosticTimeout(near.num,2)==123000);
        CHECK(adaptive.diagnosticTimeout(near.num,1)==120000);
        CHECK(adaptive.diagnosticTimeout(near.num,1,true)==120000);
        CHECK(adaptive.diagnosticReply(near.num)==120000);
        db.nodes.back().hops_away=7;CHECK(adaptive.diagnosticReply(near.num)==120000);
        CHECK(adaptive.diagnosticStep(near.num)==5700);
        db.nodes.back().hops_away=0;
        db.nodes.back().hops_away=4;CHECK(adaptive.diagnosticStep(near.num)==3900);
        config.lora.modem_preset=meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW;
        CHECK(adaptive.diagnosticStep(near.num)==23500);CHECK(adaptive.diagnosticTimeout(near.num,3)==647000);
        db.nodes.back().hops_away=7;CHECK(adaptive.diagnosticReply(near.num)==600000);CHECK(adaptive.diagnosticStep(near.num)==25000);
        db.nodes.back().hops_away=4;
        config.lora=previous;db.nodes.back().hops_away=0;
        auto start=testMillis;auto before=radio.sent.size();
        adaptive.handleIncomingCommand("{\"type\":\"CMD_POLL_ENV\",\"targetNodeId\":2575857510}");
        CHECK(radio.sent.size()==before+1);CHECK(adaptive.pending.front().timeout==120000);
        adaptive.handleIncomingCommand("{\"type\":\"CMD_PING_NODE\",\"targetNodeId\":2864434397}");
        CHECK(radio.sent.size()==before+1);CHECK(adaptive.polls.front().sent==0);
        auto state=lastEvent(adaptive,"RADIO_COMMAND_STATE");CHECK(std::string(sget(state.get(),"state"))=="queued");
        testMillis=start+2999;adaptive.runOnce();CHECK(radio.sent.size()==before+1);
        testMillis=start+3000;adaptive.runOnce();CHECK(radio.sent.size()==before+2);CHECK(adaptive.polls.front().sent==1);
        testMillis=start+120001;adaptive.runOnce();CHECK(adaptive.pending.empty());CHECK(!adaptive.polls.empty());
        auto timeout=lastEvent(adaptive,"POLL_ENV_RESULT");CHECK(cJSON_IsFalse(cJSON_GetObjectItem(timeout.get(),"success")));
        auto &ping=adaptive.polls.front();ping.replies=1;adaptive.sendDiagnosticStates();
        state=lastEvent(adaptive,"RADIO_COMMAND_STATE");CHECK(std::string(sget(state.get(),"state"))=="partial");CHECK(cJSON_IsFalse(cJSON_GetObjectItem(state.get(),"blocking")));
        config.lora=previous;
    }
    // AIR SENSOR: two typed requests; only fresh, addressed sensor packets count.
    {
        MvtWssGateway airG;airG.authenticated=true;airG.socketConnected=true;airG.authSent=true;
        airG.wsClient=esp_websocket_client_init(nullptr);airG.serverTimeMs=1700000000000ULL;airG.serverTimeTick=testMillis;
        airG.configuredRoom=moduleConfig.mqtt.username;airG.configuredKey=moduleConfig.mqtt.password;airG.configuredAddress=moduleConfig.mqtt.address;
        command(airG,"{\"type\":\"CMD_POLL_AIR\",\"targetNodeId\":2864434397}");
        CHECK(airG.airPolls.size()==1);CHECK(airG.airPolls.front().sent==1);
        auto requestPacket=radio.sent.back();
        testMillis+=1000;
        command(airG,"{\"type\":\"CMD_POLL_AIR\",\"targetNodeId\":2864434397}");
        auto busy=lastEvent(airG,"COMMAND_THROTTLED");CHECK(nget(busy.get(),"remainingSec")>0&&nget(busy.get(),"remainingSec")<45);auto firstWait=nget(busy.get(),"remainingSec");
        testMillis+=1000;
        command(airG,"{\"type\":\"CMD_POLL_ENV\",\"targetNodeId\":2864434397}");
        busy=lastEvent(airG,"COMMAND_THROTTLED");CHECK(nget(busy.get(),"remainingSec")>0&&nget(busy.get(),"remainingSec")<firstWait);
        auto envId=airG.airPolls.front().ids[0];
        meshtastic_Telemetry q=meshtastic_Telemetry_init_default;
        pb_istream_t requestStream=pb_istream_from_buffer(requestPacket.decoded.payload.bytes,requestPacket.decoded.payload.size);
        CHECK(pb_decode(&requestStream,&meshtastic_Telemetry_msg,&q));
        CHECK(q.which_variant==meshtastic_Telemetry_environment_metrics_tag);
        q=meshtastic_Telemetry_init_default;q.which_variant=meshtastic_Telemetry_environment_metrics_tag;
        q.variant.environment_metrics.has_iaq=true;q.variant.environment_metrics.iaq=0;
        q.variant.environment_metrics.has_gas_resistance=true;q.variant.environment_metrics.gas_resistance=125.5;
        auto reply=incoming(0xaabbccdd,envId+1,meshtastic_PortNum_TELEMETRY_APP);
        reply.decoded.payload.size=pb_encode_to_bytes(reply.decoded.payload.bytes,sizeof(reply.decoded.payload.bytes),&meshtastic_Telemetry_msg,&q);
        airG.handleReceived(reply);CHECK(airG.airPolls.front().values.empty());
        reply.decoded.request_id=envId;reply.id=radio.nextId++;airG.handleReceived(reply);
        CHECK(airG.airPolls.front().values.at("iaq")==0);CHECK(airG.airPolls.front().values.at("gasResistance")==125.5);
        testMillis+=airG.airPolls.front().spacing;airG.advanceAirPolls();CHECK(airG.airPolls.front().sent==2);
        requestPacket=radio.sent.back();q=meshtastic_Telemetry_init_default;
        requestStream=pb_istream_from_buffer(requestPacket.decoded.payload.bytes,requestPacket.decoded.payload.size);
        CHECK(pb_decode(&requestStream,&meshtastic_Telemetry_msg,&q));
        CHECK(q.which_variant==meshtastic_Telemetry_air_quality_metrics_tag);
        q.variant.air_quality_metrics.has_pm25_standard=true;q.variant.air_quality_metrics.pm25_standard=0;
        q.variant.air_quality_metrics.has_co2=true;q.variant.air_quality_metrics.co2=720;
        reply=incoming(0xaabbccdd,airG.airPolls.front().ids[1],meshtastic_PortNum_TELEMETRY_APP);
        reply.decoded.payload.size=pb_encode_to_bytes(reply.decoded.payload.bytes,sizeof(reply.decoded.payload.bytes),&meshtastic_Telemetry_msg,&q);
        airG.handleReceived(reply);CHECK(airG.airPolls.empty());
        auto airResult=lastEvent(airG,"AIR_SENSOR_RESULT");CHECK(cJSON_IsTrue(cJSON_GetObjectItem(airResult.get(),"success")));
        CHECK(cJSON_IsTrue(cJSON_GetObjectItem(airResult.get(),"final")));CHECK(nget(airResult.get(),"pm25")==0&&nget(airResult.get(),"co2")==720);
        command(airG,"{\"type\":\"CMD_POLL_AIR\",\"targetNodeId\":2864434397}");
        testMillis+=airG.airPolls.front().spacing;airG.advanceAirPolls();testMillis+=airG.airPolls.front().timeout+1;airG.advanceAirPolls();CHECK(airG.airPolls.empty());
        airResult=lastEvent(airG,"AIR_SENSOR_RESULT");CHECK(cJSON_IsFalse(cJSON_GetObjectItem(airResult.get(),"success")));
        command(airG,"{\"type\":\"CMD_POLL_AIR\",\"targetNodeId\":2864434397}");
        auto &bme=airG.airPolls.front();bme.values["gasResistance"]=125.5;bme.environmentReceived=true;bme.completed=1;
        testMillis+=bme.spacing;airG.advanceAirPolls();
        auto nack=incoming(0xaabbccdd,bme.ids[1],meshtastic_PortNum_ROUTING_APP);
        meshtastic_Routing noSensor=meshtastic_Routing_init_default;noSensor.which_variant=meshtastic_Routing_error_reason_tag;noSensor.error_reason=meshtastic_Routing_Error_NO_RESPONSE;
        nack.decoded.payload.size=pb_encode_to_bytes(nack.decoded.payload.bytes,sizeof(nack.decoded.payload.bytes),&meshtastic_Routing_msg,&noSensor);
        airG.handleReceived(nack);CHECK(airG.airPolls.empty());
        auto partial=lastEvent(airG,"AIR_SENSOR_RESULT");CHECK(cJSON_IsTrue(cJSON_GetObjectItem(partial.get(),"success")));CHECK(cJSON_IsTrue(cJSON_GetObjectItem(partial.get(),"final")));CHECK(cJSON_IsFalse(cJSON_GetObjectItem(partial.get(),"airReceived")));
        MvtWssGateway::Poll completed;completed.command={42,0xaabbccdd,testMillis,"ping","!aabbccdd",false};
        completed.count=2;completed.sent=2;completed.replies=1;airG.polls.push_back(completed);
        command(airG,"{\"type\":\"CMD_POLL_ENV\",\"targetNodeId\":2864434397}");
        CHECK(airG.polls.empty());CHECK(!airG.pending.empty()&&airG.pending.back().kind=="env");

    }
    // Golden MV bytes produced by the portal's actual encoder.
    if (argc > 1) {
        std::ifstream vectors(argv[1]); std::string line;
        while (std::getline(vectors, line)) {
            auto v = parse(line); const char *wire = sget(v.get(), "hex"); std::vector<uint8_t> bytes;
            for (size_t i = 0; i < strlen(wire); i += 2) { std::string b(wire + i, 2); bytes.push_back(uint8_t(strtoul(b.c_str(), nullptr, 16))); }
            CHECK(mvt::decode(bytes.data(), bytes.size(), payload));
            CHECK(payload.name == sget(v.get(), "name") && payload.comment == sget(v.get(), "comment"));
            uint8_t encoded[233]; auto size = mvt::encode(payload, encoded, sizeof(encoded));
            CHECK(size == bytes.size() && std::equal(bytes.begin(), bytes.end(), encoded));
        }
    }
    if (argc > 2) { std::ofstream transcript(argv[2]); for (auto &e : testSent) transcript << e << '\n'; }
    printf("WSS production handlers: %d checks passed\n", checks);
}
