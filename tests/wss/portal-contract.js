'use strict';
const fs = require('node:fs');
const assert = require('node:assert/strict');
const path = require('node:path');
const [relay, input] = process.argv.slice(2);
const {nextStatus} = require(path.resolve(relay,'lib/chat-ack.js'));
const rows = fs.readFileSync(input,'utf8').trim().split('\n').map(JSON.parse);
const allowed = ['AUTH_GATEWAY','GATEWAY_HEARTBEAT','NODE_UPDATE','CHAT_MESSAGE','CHAT_STATUS_UPDATE','WAYPOINT','NODE_REMOVE',
  'AIR_SENSOR_RESULT','PING_RESULT','POLL_ENV_RESULT','TRACEROUTE_RESULT','TRACEROUTE_TIMEOUT','COMMAND_THROTTLED'];
let statuses = 0;
for (const e of rows) {
  assert(allowed.includes(e.type),`unsupported event ${e.type}`);
  if (e.type === 'NODE_UPDATE') {
    assert.match(e.node.nodeKey,/^![0-9a-f]{8}(_mv)?$/);
    assert(e.node.callsign); assert(e.node.lastUpdateMs === 0 || e.node.lastUpdateMs >= 1577836800000);
  }
  if (e.type === 'CHAT_STATUS_UPDATE') {
    assert(['SENT','AWAITING_ACK','DELIVERED','FAILED','UNCONFIRMED'].includes(e.status));
    assert.equal(nextStatus('SENDING',e.status),e.status); statuses++;
  }
  if (e.type === 'CHAT_MESSAGE') {
    assert(e.message.id && e.message.senderId); assert(e.message.timestampMs >= 1577836800000);
  }
}
assert(rows.some(e=>e.type==='PING_RESULT' && e.success));
assert(rows.some(e=>e.type==='POLL_ENV_RESULT' && e.temperature===-12.5));
assert(rows.some(e=>e.type==='TRACEROUTE_RESULT' && e.outboundLastSnr===-2));
console.log(`Current portal contract: ${rows.length} actual firmware events and ${statuses} delivery statuses accepted`);

assert(rows.some(e=>e.type==='AIR_SENSOR_RESULT'&&e.final&&e.success&&e.co2===720&&e.gasResistance===125.5));
