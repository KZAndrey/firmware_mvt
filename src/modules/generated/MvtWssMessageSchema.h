// Generated; do not edit. Source SHA-256: 11d0ebdd371c5e288f3d6a4a444a393afb43c8c17057bfc45bf7e3a5534304ec
#pragma once
#include <cstddef>
#include <cstdint>
namespace MvtWssSchema {
struct Field { const char *name; uint8_t id,type,sub; bool repeated,optional; };
struct Message { const Field *fields; size_t count; };
static const Field fields_Heartbeat[] = {
    {"timestamp_ms",1,4,0,false,false},
};
static const Field fields_HeartbeatAck[] = {
    {"timestamp_ms",1,4,0,false,false},
};
static const Field fields_FirmwareChunk[] = {
    {"room_id",1,9,0,false,false},
    {"node_id",2,7,0,false,false},
    {"job_id",3,9,0,false,false},
    {"offset",4,13,0,false,false},
    {"data",5,12,0,false,false},
};
static const Field fields_AuthGateway[] = {
    {"room",1,9,0,false,false},
    {"relayKey",2,9,0,false,false},
    {"nodeId",3,7,0,false,false},
    {"nodeName",4,9,0,false,false},
    {"firmwareVersion",5,9,0,false,false},
};
static const Field fields_AuthGatewaySuccess[] = {
    {"timestamp",1,4,0,false,false},
};
static const Field fields_Error[] = {
    {"code",1,9,0,false,false},
    {"message",2,9,0,false,false},
};
static const Field fields_Diagnostic[] = {
    {"bootId",1,7,0,false,false},
    {"sequence",2,13,0,false,false},
    {"uptimeMs",3,13,0,false,false},
    {"stage",4,9,0,false,false},
    {"ret",5,17,0,false,false},
    {"error",6,17,0,false,false},
    {"socketError",7,17,0,false,false},
    {"tlsError",8,17,0,false,false},
    {"tlsCode",9,17,0,false,false},
    {"tlsFlags",10,17,0,false,false},
    {"opcode",11,17,0,false,false},
    {"payload",12,17,0,false,false},
    {"offset",13,17,0,false,false},
    {"duringSession",14,8,0,false,false},
    {"freeHeap",15,13,0,false,true},
    {"largestBlock",16,13,0,false,true},
    {"minimumHeap",17,13,0,false,true},
};
static const Field fields_BootDiagnostic[] = {
    {"bootId",1,7,0,false,false},
    {"resetReason",2,13,0,false,false},
    {"wakeCause",3,13,0,false,false},
    {"bootCount",4,13,0,false,false},
    {"previousSleepUptimeMs",5,13,0,false,false},
    {"previousSleepDurationMs",6,13,0,false,false},
    {"sleepIntent",7,13,0,false,false},
};
static const Field fields_Node[] = {
    {"nodeId",1,7,0,false,false},
    {"nodeKey",2,9,0,false,false},
    {"nodeHex",3,9,0,false,false},
    {"callsign",4,9,0,false,false},
    {"longName",5,9,0,false,false},
    {"shortName",6,9,0,false,false},
    {"pkiSource",7,9,0,false,false},
    {"pkiGatewayId",8,7,0,false,false},
    {"pkiState",9,9,0,false,false},
    {"lat",10,1,0,false,true},
    {"lon",11,1,0,false,true},
    {"alt",12,1,0,false,true},
    {"radioBattery",13,1,0,false,true},
    {"battery",14,1,0,false,true},
    {"radioCharging",15,8,0,false,true},
    {"voltage",16,1,0,false,true},
    {"channelUtilization",17,1,0,false,true},
    {"airUtilTx",18,1,0,false,true},
    {"hops",19,13,0,false,true},
    {"snr",20,1,0,false,true},
    {"rssi",21,17,0,false,true},
    {"lastUpdateMs",22,4,0,false,false},
    {"temp",23,1,0,false,true},
    {"humidity",24,1,0,false,true},
    {"pressure",25,1,0,false,true},
    {"iaq",26,1,0,false,true},
    {"gasResistance",27,1,0,false,true},
    {"co2",28,1,0,false,true},
    {"pm25",29,1,0,false,true},
    {"heartRate",30,1,0,false,true},
    {"gpsFix",31,8,0,false,true},
    {"lastPositionMs",32,4,0,false,false},
    {"isMovetain",33,8,0,false,true},
    {"isBeaconActive",34,8,0,false,true},
    {"mvColor",35,9,0,false,false},
    {"mvIcon",36,9,0,false,false},
    {"phoneBattery",37,1,0,false,true},
    {"phoneCharging",38,8,0,false,true},
    {"clearedFields",39,13,0,true,false},
};
static const Field fields_GatewayInfo[] = {
    {"node",1,11,8,false,false},
    {"nodeName",2,9,0,false,false},
    {"firmwareVersion",3,9,0,false,false},
    {"radioConnected",4,8,0,false,false},
    {"txEnabled",5,8,0,false,false},
    {"hopLimit",6,13,0,false,false},
    {"radioHopLimit",7,13,0,false,false},
    {"frequency",8,9,0,false,false},
    {"channelName",9,9,0,false,false},
    {"region",10,9,0,false,false},
    {"modemPreset",11,9,0,false,false},
    {"otaSupported",12,8,0,false,false},
    {"otaGeneration",13,13,0,false,false},
    {"lastDisconnectDiag",14,11,6,false,false},
    {"lastAttemptDiag",15,11,6,false,false},
    {"bootDiagnostic",16,11,7,false,false},
    {"sampledAt",17,4,0,false,false},
};
static const Field fields_NodeUpdate[] = {
    {"node",1,11,8,false,false},
    {"snapshot",2,8,0,false,false},
    {"nodeKey",3,9,0,false,false},
};
static const Field fields_BatchUpdate[] = {
    {"nodes",1,11,8,true,false},
    {"isSnapshot",2,8,0,false,false},
};
static const Field fields_NodeRadioActivity[] = {
    {"from",1,7,0,false,false},
    {"id",2,7,0,false,false},
    {"rxAt",3,4,0,false,false},
};
static const Field fields_NodeRemove[] = {
    {"nodeKey",1,9,0,false,false},
};
static const Field fields_Waypoint[] = {
    {"title",1,9,0,false,false},
    {"comment",2,9,0,false,false},
    {"latitude",3,1,0,false,false},
    {"longitude",4,1,0,false,false},
    {"altitude",5,1,0,false,true},
    {"timestampSec",6,4,0,false,false},
    {"isRadio",7,8,0,false,false},
    {"isTarget",8,8,0,false,false},
    {"isSos",9,8,0,false,false},
    {"id",10,9,0,false,false},
    {"originName",11,9,0,false,false},
    {"forwardedBy",12,9,0,false,false},
    {"viaMecc",13,8,0,false,false},
};
static const Field fields_WaypointEvent[] = {
    {"waypoint",1,11,14,false,false},
    {"snapshot",2,8,0,false,false},
};
static const Field fields_Chat[] = {
    {"id",1,7,0,false,false},
    {"senderId",2,7,0,false,false},
    {"recipientId",3,7,0,false,false},
    {"encryption",4,9,0,false,false},
    {"senderCallsign",5,9,0,false,false},
    {"text",6,9,0,false,false},
    {"viaMecc",7,8,0,false,false},
    {"timestampMs",8,4,0,false,false},
    {"isSos",9,8,0,false,false},
    {"hops",10,13,0,false,false},
    {"status",11,9,0,false,false},
    {"waypoint",12,11,14,false,false},
    {"sender",13,9,0,false,false},
    {"isRadio",14,8,0,false,false},
};
static const Field fields_ChatMessage[] = {
    {"message",1,11,16,false,false},
};
static const Field fields_ChatStatusUpdate[] = {
    {"id",1,7,0,false,false},
    {"status",2,9,0,false,false},
    {"encryption",3,9,0,false,false},
    {"errorCode",4,9,0,false,false},
};
static const Field fields_RadioKeyCommand[] = {
    {"requestId",1,9,0,false,false},
    {"nodeId",2,7,0,false,false},
    {"gatewayId",3,7,0,false,false},
    {"action",4,9,0,false,false},
    {"publicKey",5,12,0,false,false},
    {"expectedKey",6,12,0,false,false},
    {"verified",7,8,0,false,false},
};
static const Field fields_RadioKeyResult[] = {
    {"requestId",1,9,0,false,false},
    {"nodeId",2,7,0,false,false},
    {"gatewayId",3,7,0,false,false},
    {"ok",4,8,0,false,false},
    {"publicKey",5,12,0,false,false},
    {"message",6,9,0,false,false},
};
static const Field fields_RadioCommand[] = {
    {"targetNodeId",1,7,0,false,false},
    {"nodeKey",2,9,0,false,false},
    {"isVirtual",3,8,0,false,false},
};
static const Field fields_HopLimit[] = {
    {"hopLimit",1,13,0,false,false},
};
static const Field fields_WaypointSelection[] = {
    {"latitude",1,1,0,false,true},
    {"longitude",2,1,0,false,true},
    {"title",3,9,0,false,false},
    {"timestampSec",4,4,0,false,false},
    {"id",5,9,0,false,false},
};
static const Field fields_TxCommand[] = {
    {"id",1,7,0,false,false},
    {"recipientId",2,7,0,false,false},
    {"senderCallsign",3,9,0,false,false},
    {"text",4,9,0,false,false},
    {"viaMecc",5,8,0,false,false},
    {"isSos",6,8,0,false,false},
    {"waypoint",7,11,14,false,false},
};
static const Field fields_FirmwareManifest[] = {
    {"schema",1,13,0,false,false},
    {"model",2,9,0,false,false},
    {"chip",3,9,0,false,false},
    {"releaseId",4,9,0,false,false},
    {"version",5,9,0,false,false},
    {"generation",6,13,0,false,false},
    {"size",7,13,0,false,false},
    {"sha256",8,12,0,false,false},
    {"imagePath",9,9,0,false,false},
    {"signature",10,12,0,false,false},
};
static const Field fields_FirmwareUpdate[] = {
    {"roomId",1,9,0,false,false},
    {"nodeId",2,7,0,false,false},
    {"jobId",3,9,0,false,false},
    {"manifest",4,11,26,false,false},
};
static const Field fields_FirmwareUpdateState[] = {
    {"jobId",1,9,0,false,false},
    {"nodeId",2,7,0,false,false},
    {"state",3,9,0,false,false},
    {"percent",4,13,0,false,false},
    {"downloadOffset",5,13,0,false,true},
    {"error",6,9,0,false,false},
};
static const Field fields_RouteHop[] = {
    {"nodeId",1,7,0,false,false},
    {"nodeHex",2,9,0,false,false},
    {"callsign",3,9,0,false,false},
    {"snr",4,1,0,false,true},
};
static const Field fields_DiagnosticResult[] = {
    {"targetNodeId",1,7,0,false,false},
    {"targetNodeHex",2,9,0,false,false},
    {"nodeKey",3,9,0,false,false},
    {"isVirtual",4,8,0,false,false},
    {"success",5,8,0,false,false},
    {"positionReceived",6,8,0,false,true},
    {"telemetryReceived",7,8,0,false,true},
    {"nodeInfoReceived",8,8,0,false,true},
    {"durationMs",9,13,0,false,true},
    {"rttMs",10,13,0,false,true},
    {"snr",11,1,0,false,true},
    {"hops",12,13,0,false,true},
    {"batteryPct",13,1,0,false,true},
    {"temperature",14,1,0,false,true},
    {"humidity",15,1,0,false,true},
    {"pressure",16,1,0,false,true},
    {"iaq",17,1,0,false,true},
    {"gasResistance",18,1,0,false,true},
    {"co2",19,1,0,false,true},
    {"pm25",20,1,0,false,true},
    {"outboundKnown",21,8,0,false,true},
    {"returnRouteKnown",22,8,0,false,true},
    {"outbound",23,11,29,true,false},
    {"returnRoute",24,11,29,true,false},
    {"outboundLastSnr",25,1,0,false,true},
    {"returnLastSnr",26,1,0,false,true},
    {"final",27,8,0,false,true},
    {"partial",28,8,0,false,true},
    {"environmentReceived",29,8,0,false,true},
    {"airReceived",30,8,0,false,true},
    {"code",31,9,0,false,false},
};
static const Field fields_CommandState[] = {
    {"targetNodeId",1,7,0,false,false},
    {"nodeKey",2,9,0,false,false},
    {"command",3,9,0,false,false},
    {"state",4,9,0,false,false},
    {"remainingMs",5,13,0,false,false},
    {"cooldownRemainingMs",6,13,0,false,false},
    {"blocking",7,8,0,false,false},
};
static const Field fields_CommandThrottled[] = {
    {"targetNodeId",1,7,0,false,false},
    {"nodeKey",2,9,0,false,false},
    {"command",3,9,0,false,false},
    {"reason",4,9,0,false,false},
    {"remainingSec",5,13,0,false,false},
};
static const Message messages[] = {
    {fields_Heartbeat,1},
    {fields_HeartbeatAck,1},
    {fields_FirmwareChunk,5},
    {fields_AuthGateway,5},
    {fields_AuthGatewaySuccess,1},
    {fields_Error,2},
    {fields_Diagnostic,17},
    {fields_BootDiagnostic,7},
    {fields_Node,39},
    {fields_GatewayInfo,17},
    {fields_NodeUpdate,3},
    {fields_BatchUpdate,2},
    {fields_NodeRadioActivity,3},
    {fields_NodeRemove,1},
    {fields_Waypoint,13},
    {fields_WaypointEvent,2},
    {fields_Chat,14},
    {fields_ChatMessage,1},
    {fields_ChatStatusUpdate,4},
    {fields_RadioKeyCommand,7},
    {fields_RadioKeyResult,6},
    {fields_RadioCommand,3},
    {fields_HopLimit,1},
    {nullptr,0},
    {fields_WaypointSelection,5},
    {fields_TxCommand,7},
    {fields_FirmwareManifest,10},
    {fields_FirmwareUpdate,4},
    {fields_FirmwareUpdateState,6},
    {fields_RouteHop,4},
    {fields_DiagnosticResult,31},
    {fields_CommandState,7},
    {fields_CommandThrottled,5},
};
static const uint8_t bodies[] = {0,3,4,5,0,1,9,10,12,13,17,18,15,31,30,30,30,30,30,32,20,28,19,27,2,22,23,24,24,25,25,21,21,21,21,11};
static const char *const kinds[] = {"INVALID","AUTH_GATEWAY","AUTH_GATEWAY_SUCCESS","ERROR","GATEWAY_HEARTBEAT","GATEWAY_HEARTBEAT_ACK","GATEWAY_INFO","NODE_UPDATE","NODE_RADIO_ACTIVITY","NODE_REMOVE","CHAT_MESSAGE","CHAT_STATUS_UPDATE","WAYPOINT","RADIO_COMMAND_STATE","AIR_SENSOR_RESULT","PING_RESULT","POLL_ENV_RESULT","TRACEROUTE_RESULT","TRACEROUTE_TIMEOUT","COMMAND_THROTTLED","RADIO_KEY_RESULT","FIRMWARE_UPDATE_STATE","CMD_RADIO_KEY","CMD_FIRMWARE_UPDATE","CMD_FIRMWARE_CHUNK","CMD_SET_WSS_HOP_LIMIT","CMD_REFRESH_GATEWAY","WAYPOINT_DELETED","CMD_DELETE_WAYPOINT","CMD_TX_LORA_CHAT","CMD_TX_LORA_WAYPOINT","CMD_POLL_AIR","CMD_PING_NODE","CMD_POLL_ENV","CMD_TRACEROUTE","BATCH_UPDATE"};
}
