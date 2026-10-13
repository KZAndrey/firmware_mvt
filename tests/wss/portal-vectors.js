'use strict';
const fs = require('node:fs');
const path = require('node:path');
const relay = process.argv[2], output = process.argv[3];
const { encodeMovetainPayload } = require(path.resolve(relay, 'lib/meshtastic-mqtt.js'));
// Extract protobuf bytes without reimplementing the production MV encoder.
function bytes(buf, field) {
  let i = 0;
  const vint = () => { let n = 0, shift = 0, b; do { b = buf[i++]; n += (b & 127) * 2 ** shift; shift += 7; } while (b & 128); return n; };
  while (i < buf.length) {
    const tag = vint(), wire = tag & 7;
    if (wire === 0) { vint(); continue; }
    const size = wire === 2 ? vint() : wire === 5 ? 4 : 8;
    const value = buf.subarray(i, i + size); i += size;
    if ((tag >>> 3) === field) return value;
  }
  throw new Error('field missing');
}
const cases = [
  {senderCallsign:'HQ', text:'hello "quoted"\nnext', isChatMessage:true},
  {senderCallsign:'Штаб', text:'Привет 👋', isChatMessage:true, isSos:true},
  {senderCallsign:'Target', text:'точка', lat:-43.123456, lon:76.987654, isWaypoint:true, icon:'TGT', bat:80, chg:true},
  {senderCallsign:'Маяк', lat:90, lon:-180, color:'GRN', icon:'RAD', bat:100},
];
const rows = cases.map(c => {
  const env = encodeMovetainPayload({from:0x11223344,to:0xaabbccdd,id:42,channelName:'LongFast',channel:2,hopLimit:4,...c});
  const mv = bytes(bytes(bytes(env,1),4),2);
  return JSON.stringify({hex:mv.toString('hex'),name:c.senderCallsign,comment:c.text||''});
});
fs.writeFileSync(output, rows.join('\n')+'\n');
console.log(`Portal MV encoder: ${rows.length} golden vectors generated`);
