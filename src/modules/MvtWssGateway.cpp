#include "configuration.h"

#if HAS_MVT_WSS_GATEWAY

#include "MvtWssGateway.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "Power.h"
#include "PowerStatus.h"
#include "Router.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"
#include <Arduino.h>
#include <pb_decode.h>

MvtWssGateway *mvtWssGateway = nullptr;

MvtWssGateway::MvtWssGateway()
    : MeshModule("mvt_wss"), concurrency::OSThread("mvt_wss", 1000)
{
    mvtWssGateway = this;
    isPromiscuous = true;
    encryptedOk = false;
    loopbackOk = false;
    LOG_INFO("MVT WSS Gateway initialized. Awaiting network & credentials...");
}

MvtWssGateway::~MvtWssGateway()
{
    disconnectWs();
    mvtWssGateway = nullptr;
}

bool MvtWssGateway::wantPacket(const meshtastic_MeshPacket *p)
{
    // Inspect all received packets to bridge to Movetain WSS
    return true;
}

void MvtWssGateway::handleWsEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    MvtWssGateway *self = static_cast<MvtWssGateway *>(handler_args);
    if (!self) return;

    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        self->onWsConnected();
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        self->onWsDisconnected();
        break;
    case WEBSOCKET_EVENT_DATA:
        if (data && data->data_ptr && data->data_len > 0) {
            self->onWsData(data->data_ptr, data->data_len);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        LOG_WARN("MVT WSS Error event");
        break;
    default:
        break;
    }
}

void MvtWssGateway::onWsConnected()
{
    LOG_INFO("MVT WSS WebSocket connected! Sending AUTH_GATEWAY...");
    wsConnected = true;
    sendAuthGateway();
    lastHeartbeatMs = millis();
}

void MvtWssGateway::onWsDisconnected()
{
    LOG_WARN("MVT WSS WebSocket disconnected");
    wsConnected = false;
}

void MvtWssGateway::onWsData(const char *data, int len)
{
    if (len <= 0 || !data) return;

    // Null-terminate safely for parsing
    char *buf = (char *)malloc(len + 1);
    if (!buf) return;
    memcpy(buf, data, len);
    buf[len] = '\0';

    handleIncomingCommand(buf);
    free(buf);
}

bool MvtWssGateway::sendJson(const char *jsonStr)
{
    if (!wsConnected || !wsClient || !jsonStr) return false;
    int len = strlen(jsonStr);
    int res = esp_websocket_client_send_text(wsClient, jsonStr, len, portMAX_DELAY);
    return res >= 0;
}

void MvtWssGateway::connectWs()
{
    disconnectWs();

    const char *cfgRoom = moduleConfig.mqtt.username;
    const char *cfgKey  = moduleConfig.mqtt.password;
    const char *cfgAddr = moduleConfig.mqtt.address;

    if (!cfgRoom || strlen(cfgRoom) == 0 || !cfgKey || strlen(cfgKey) == 0) {
        LOG_DEBUG("MVT WSS: Room ID or Relay Key empty in MQTT settings");
        return;
    }

    String url = cfgAddr;
    if (url.length() == 0) {
        url = "wss://movetain.app/ws";
    } else if (!url.startsWith("ws://") && !url.startsWith("wss://")) {
        url = "wss://" + url;
        if (!url.endsWith("/ws")) {
            url += "/ws";
        }
    }

    LOG_INFO("Connecting MVT WSS: %s (Room: %s)...", url.c_str(), cfgRoom);

    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = url.c_str();
    ws_cfg.task_stack = 4096;
    ws_cfg.buffer_size = 2048;
    ws_cfg.ping_interval_sec = 20;
    ws_cfg.pingpong_timeout_sec = 45;
    ws_cfg.disable_auto_reconnect = false;

    wsClient = esp_websocket_client_init(&ws_cfg);
    if (!wsClient) {
        LOG_ERROR("Failed to init esp_websocket_client");
        return;
    }

    esp_websocket_register_events(wsClient, WEBSOCKET_EVENT_ANY, handleWsEvent, this);
    esp_websocket_client_start(wsClient);
}

void MvtWssGateway::disconnectWs()
{
    if (wsClient) {
        esp_websocket_client_stop(wsClient);
        esp_websocket_client_destroy(wsClient);
        wsClient = nullptr;
    }
    wsConnected = false;
}

void MvtWssGateway::sendAuthGateway()
{
    char buf[512];
    uint32_t myNode = nodeDB ? nodeDB->getNodeNum() : 0;
    char hex[16];
    snprintf(hex, sizeof(hex), "%08x", myNode);

    uint32_t batPct = powerStatus ? powerStatus->getBatteryChargePercent() : 0;
    float volt = powerStatus ? (float)powerStatus->getBatteryVoltageMv() / 1000.0f : 0.0f;
    uint32_t freeHeap = ESP.getFreeHeap();

    snprintf(buf, sizeof(buf),
        "{\"type\":\"AUTH_GATEWAY\",\"room\":\"%s\",\"relayKey\":\"%s\",\"adminKey\":\"%s\",\"source\":\"Heltec Stick V3\",\"gatewayInfo\":{\"radioConnected\":true,\"nodeHex\":\"%s\",\"nodeId\":%u,\"phoneBattery\":%u,\"voltage\":%.2f,\"freeHeap\":%u,\"transport\":\"Direct WSS Gateway (WiFi)\"}}",
        moduleConfig.mqtt.username, moduleConfig.mqtt.password, moduleConfig.mqtt.password,
        hex, myNode, batPct, volt, freeHeap
    );
    sendJson(buf);
}

void MvtWssGateway::sendHeartbeat()
{
    char buf[512];
    uint32_t myNode = nodeDB ? nodeDB->getNodeNum() : 0;
    char hex[16];
    snprintf(hex, sizeof(hex), "%08x", myNode);

    uint32_t batPct = powerStatus ? powerStatus->getBatteryChargePercent() : 0;
    float volt = powerStatus ? (float)powerStatus->getBatteryVoltageMv() / 1000.0f : 0.0f;
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();

    snprintf(buf, sizeof(buf),
        "{\"type\":\"GATEWAY_HEARTBEAT\",\"timestamp\":%lu,\"gatewayInfo\":{\"radioConnected\":true,\"nodeHex\":\"%s\",\"nodeId\":%u,\"radioBattery\":%u,\"voltage\":%.2f,\"freeHeap\":%u,\"minFreeHeap\":%u}}",
        millis(), hex, myNode, batPct, volt, freeHeap, minFreeHeap
    );
    sendJson(buf);
}

ProcessMessage MvtWssGateway::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (!wsConnected) return ProcessMessage::CONTINUE;

    uint32_t senderId = mp.from;
    char senderHex[16];
    snprintf(senderHex, sizeof(senderHex), "%08x", senderId);

    const char *callsign = senderHex;
    if (nodeDB) {
        auto *n = nodeDB->getMeshNode(senderId);
        if (n && n->has_user && n->user.short_name[0]) {
            callsign = n->user.short_name;
        }
    }

    if (mp.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
        char textPayload[256];
        size_t copyLen = min((size_t)mp.decoded.payload.size, sizeof(textPayload) - 1);
        memcpy(textPayload, mp.decoded.payload.bytes, copyLen);
        textPayload[copyLen] = '\0';

        char jsonBuf[512];
        snprintf(jsonBuf, sizeof(jsonBuf),
            "{\"type\":\"CHAT_MESSAGE\",\"message\":{\"id\":%u,\"senderId\":%u,\"senderCallsign\":\"%s\",\"recipientId\":%u,\"text\":\"%s\",\"timestampMs\":%lu,\"isSos\":false,\"hops\":%u,\"status\":\"DELIVERED\"}}",
            mp.id, senderId, callsign, mp.to, textPayload, millis(), mp.hop_limit
        );
        sendJson(jsonBuf);
    }
    else if (mp.decoded.portnum == meshtastic_PortNum_POSITION_APP) {
        meshtastic_Position pos = meshtastic_Position_init_default;
        pb_istream_t stream = pb_istream_from_buffer(mp.decoded.payload.bytes, mp.decoded.payload.size);
        if (pb_decode(&stream, &meshtastic_Position_msg, &pos)) {
            double lat = pos.latitude_i / 1e7;
            double lon = pos.longitude_i / 1e7;
            int32_t alt = pos.altitude;

            char jsonBuf[512];
            snprintf(jsonBuf, sizeof(jsonBuf),
                "{\"type\":\"NODE_UPDATE\",\"nodeKey\":\"!%s\",\"node\":{\"nodeKey\":\"!%s\",\"nodeId\":%u,\"nodeHex\":\"%s\",\"callsign\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"alt\":%d,\"lastUpdateMs\":%lu}}",
                senderHex, senderHex, senderId, senderHex, callsign, lat, lon, alt, millis()
            );
            sendJson(jsonBuf);
        }
    }
    else if (mp.decoded.portnum == meshtastic_PortNum_WAYPOINT_APP) {
        meshtastic_Waypoint wp = meshtastic_Waypoint_init_default;
        pb_istream_t stream = pb_istream_from_buffer(mp.decoded.payload.bytes, mp.decoded.payload.size);
        if (pb_decode(&stream, &meshtastic_Waypoint_msg, &wp)) {
            double lat = wp.latitude_i / 1e7;
            double lon = wp.longitude_i / 1e7;
            bool isTarget = (strstr(wp.name, "Target") != nullptr) || (strstr(wp.description, "Target") != nullptr);

            char jsonBuf[512];
            snprintf(jsonBuf, sizeof(jsonBuf),
                "{\"type\":\"WAYPOINT\",\"waypoint\":{\"title\":\"%s\",\"comment\":\"%s\",\"latitude\":%.6f,\"longitude\":%.6f,\"altitude\":0,\"isRadio\":true,\"isTarget\":%s}}",
                wp.name, wp.description, lat, lon, isTarget ? "true" : "false"
            );
            sendJson(jsonBuf);
        }
    }

    return ProcessMessage::CONTINUE;
}

void MvtWssGateway::handleIncomingCommand(const char *jsonStr)
{
    if (!jsonStr) return;

    // Check for CMD_TX_LORA_CHAT: {"type":"CMD_TX_LORA_CHAT","recipientId":...,"text":"..."}
    if (strstr(jsonStr, "CMD_TX_LORA_CHAT") != nullptr) {
        const char *textKey = strstr(jsonStr, "\"text\":");
        if (textKey) {
            textKey += 7;
            while (*textKey == ' ' || *textKey == '\"') textKey++;
            const char *endQuote = strchr(textKey, '\"');
            if (endQuote && endQuote > textKey) {
                size_t txtLen = endQuote - textKey;
                if (txtLen > 150) txtLen = 150;

                meshtastic_MeshPacket *p = router->allocForSending();
                if (p) {
                    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
                    p->to = 0xFFFFFFFF; // Broadcast or parse recipientId
                    p->from = nodeDB ? nodeDB->getNodeNum() : 0;
                    p->hop_limit = config.lora.hop_limit > 0 ? config.lora.hop_limit : 3;
                    memcpy(p->decoded.payload.bytes, textKey, txtLen);
                    p->decoded.payload.size = txtLen;

                    service->sendToMesh(p, RX_SRC_LOCAL, true);
                    LOG_INFO("Downlink: Sent text to LoRa mesh (len=%d)", txtLen);
                }
            }
        }
    }

    // Check for CMD_TX_LORA_WAYPOINT: {"type":"CMD_TX_LORA_WAYPOINT","waypoint":{"title":"...","latitude":...,"longitude":...}}
    if (strstr(jsonStr, "CMD_TX_LORA_WAYPOINT") != nullptr) {
        const char *latKey = strstr(jsonStr, "\"latitude\":");
        const char *lonKey = strstr(jsonStr, "\"longitude\":");
        if (latKey && lonKey) {
            double lat = atof(latKey + 11);
            double lon = atof(lonKey + 12);
            char title[32] = "Target";
            const char *titleKey = strstr(jsonStr, "\"title\":");
            if (titleKey) {
                titleKey += 8;
                while (*titleKey == ' ' || *titleKey == '\"') titleKey++;
                const char *endQuote = strchr(titleKey, '\"');
                if (endQuote && endQuote > titleKey) {
                    size_t tLen = min((size_t)(endQuote - titleKey), sizeof(title) - 1);
                    memcpy(title, titleKey, tLen);
                    title[tLen] = '\0';
                }
            }

            meshtastic_Waypoint wp = meshtastic_Waypoint_init_default;
            wp.id = millis() / 1000;
            strncpy(wp.name, title, sizeof(wp.name) - 1);
            wp.latitude_i = (int32_t)(lat * 1e7);
            wp.longitude_i = (int32_t)(lon * 1e7);
            wp.expire = UINT32_MAX;

            meshtastic_MeshPacket *p = router->allocForSending();
            if (p) {
                p->decoded.portnum = meshtastic_PortNum_WAYPOINT_APP;
                p->to = 0xFFFFFFFF;
                p->from = nodeDB ? nodeDB->getNodeNum() : 0;
                p->hop_limit = config.lora.hop_limit > 0 ? config.lora.hop_limit : 3;
                p->decoded.payload.size = pb_encode_to_bytes(p->decoded.payload.bytes, sizeof(p->decoded.payload.bytes), &meshtastic_Waypoint_msg, &wp);

                service->sendToMesh(p, RX_SRC_LOCAL, true);
                LOG_INFO("Downlink: Transmitted Target Point '%s' (%.5f, %.5f) to LoRa mesh", title, lat, lon);
            }
        }
    }
}

int32_t MvtWssGateway::runOnce()
{
    if (!moduleConfig.mqtt.enabled) {
        if (wsConnected) disconnectWs();
        return 5000;
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (wsConnected) disconnectWs();
        return 2000;
    }

    if (!wsConnected) {
        if (millis() - lastConnectAttemptMs > 8000) {
            lastConnectAttemptMs = millis();
            connectWs();
        }
        return 2000;
    }

    // Keep-alive heartbeat every 15 seconds
    if (millis() - lastHeartbeatMs > 15000) {
        lastHeartbeatMs = millis();
        sendHeartbeat();
    }

    return 1000;
}

#endif // HAS_MVT_WSS_GATEWAY
