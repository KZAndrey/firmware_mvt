'use strict';
// Real current Relay websocket handlers, temporary storage, mocked signed auth.
const fs = require('node:fs'), path = require('node:path'), os = require('node:os');
const assert = require('node:assert/strict'), crypto = require('node:crypto');
const {createRequire} = require('node:module');
const [relayArg, transcript] = process.argv.slice(2), relay = path.resolve(relayArg);
const load = createRequire(path.join(relay,'server.js'));
const WebSocket = load('ws');
const dataDir = fs.mkdtempSync(path.join(os.tmpdir(),'mvt-wss-compat-'));
Object.assign(process.env,{DATA_DIR:dataDir,STORAGE_SECRET:'test-only-wss-compat-secret',MQTT_ENABLED:'false',FIREBASE_PROJECT_ID:'movetain-b8468'});
const api = load('./server.js');
const {publicKey,privateKey} = crypto.generateKeyPairSync('rsa',{modulusLength:2048});
api.setMockGoogleCerts({wss:publicKey.export({type:'spki',format:'pem'})},Date.now()+3600000);
function token(uid) {
  const now=Math.floor(Date.now()/1000), project=process.env.FIREBASE_PROJECT_ID;
  const h=Buffer.from(JSON.stringify({alg:'RS256',kid:'wss',typ:'JWT'})).toString('base64url');
  const p=Buffer.from(JSON.stringify({iss:`https://securetoken.google.com/${project}`,aud:project,sub:uid,user_id:uid,
    auth_time:now,iat:now,exp:now+3600,email:'wss@test.local',email_verified:true})).toString('base64url');
  return `${h}.${p}.${crypto.sign('RSA-SHA256',Buffer.from(`${h}.${p}`),privateKey).toString('base64url')}`;
}
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn) { const deadline=Date.now()+5000; while (!fn()) { if(Date.now()>deadline)throw Error('timeout'); await delay(10); } }
async function connect(url) {
  const ws=new WebSocket(url), messages=[]; ws.on('message',b=>messages.push(JSON.parse(b.toString())));
  await new Promise((resolve,reject)=>{ws.once('open',resolve);ws.once('error',reject);});
  return {ws,messages,send:e=>ws.send(JSON.stringify(e))};
}
(async()=>{
  let gateway, coord, owner;
  try {
    const room=api.getOrCreateRoom('ROOM1','relay-secret','coord-secret','spec-secret',{ownerUid:'test-owner',name:'WSS Test'});
    room.state.wssHopLimit=2;
    // This fixture exercises an approved coordinator; new invitations require owner approval.
    room.guestPermissions = {'test-coordinator': {access: {role:'coordinator', pending:false, revoked:false}}};
    await new Promise(resolve=>api.server.listen(0,'127.0.0.1',resolve));
    const url=`ws://127.0.0.1:${api.server.address().port}/ws`;
    gateway=await connect(url); const rows=fs.readFileSync(transcript,'utf8').trim().split('\n').map(JSON.parse);
    gateway.send(rows.find(e=>e.type==='AUTH_GATEWAY'));
    await until(()=>gateway.messages.some(e=>e.type==='AUTH_GATEWAY_SUCCESS'));
    const advertisedHops=rows.find(e=>e.type==='AUTH_GATEWAY').gatewayInfo.hopLimit;
    assert.equal(gateway.messages.find(e=>e.type==='AUTH_GATEWAY_SUCCESS').wssHopLimit,advertisedHops);
    assert.equal(room.state.wssHopLimit,advertisedHops);
    coord=await connect(url); coord.send({type:'AUTH',room:'ROOM1',key:'coord-secret',token:token('test-coordinator')});
    await until(()=>coord.messages.some(e=>e.type==='INIT_STATE'));
    assert(api.gatewayStatus(room).online);
    // Actual produced node telemetry survives current server sanitizers/storage.
    for(const e of rows.filter(e=>['NODE_UPDATE','CHAT_MESSAGE','AIR_SENSOR_RESULT','PING_RESULT','POLL_ENV_RESULT','TRACEROUTE_RESULT'].includes(e.type))) {
      // Rebase the deterministic host clock fixture to the live test server.
      if(e.node && e.node.lastUpdateMs)e.node.lastUpdateMs=Date.now(); if(e.message)e.message.timestampMs=Date.now(); gateway.send(e);
    }
    await until(()=>room.state.participants.has('!aabbccdd_mv'));
    const node=room.state.participants.get('!aabbccdd_mv');
    assert.equal(node.heartRate,101); assert.equal(node.alt,-20); assert.equal(node.callsign,'Offline');
    assert(node.lastUpdateMs>=1577836800000); assert(room.state.trackHistory.get('!aabbccdd_mv').length>0);
    await until(()=>coord.messages.some(e=>e.type==='AIR_SENSOR_RESULT'&&e.final&&e.success&&e.co2===720));
    await until(()=>coord.messages.filter(e=>e.type==='TRACEROUTE_RESULT').length===rows.filter(e=>e.type==='TRACEROUTE_RESULT').length);
    const traces=coord.messages.filter(e=>e.type==='TRACEROUTE_RESULT');
    assert(traces.some(e=>e.durationMs===1234&&e.asymmetric===true&&e.outboundHops===1&&e.returnHops===1));
    assert(traces.some(e=>e.durationMs===200&&e.asymmetric===false&&e.outboundHops===3&&e.returnHops===3));
    assert(traces.some(e=>e.returnRouteKnown===false&&e.asymmetric===null&&e.returnHops===null));
    assert(traces.every(e=>e.transport==='WSS'&&e.receivedAt>=1577836800000));
    // Historical/partial snapshots cannot erase newer position or full name.
    const nativeKey='!aabbccdd', liveTime=Date.now();
    gateway.send({type:'NODE_UPDATE',node:{nodeKey:nativeKey,nodeId:0xaabbccdd,callsign:'A',
      longName:'Field station',lat:49.95,lon:82.63,lastUpdateMs:liveTime}});
    await until(()=>room.state.participants.get(nativeKey)?.longName==='Field station');
    const before=coord.messages.filter(e=>e.type==='NODE_UPDATE'&&e.nodeKey===nativeKey).length;
    gateway.send({type:'NODE_UPDATE',snapshot:true,node:{nodeKey:nativeKey,nodeId:0xaabbccdd,callsign:'A',
      lat:49,lon:82,lastUpdateMs:1700000000000}});
    await until(()=>coord.messages.filter(e=>e.type==='NODE_UPDATE'&&e.nodeKey===nativeKey).length>before);
    assert.equal(room.state.participants.get(nativeKey).lat,49.95);
    assert.equal(room.state.participants.get(nativeKey).longName,'Field station');
    assert.equal(room.state.participants.get(nativeKey).lastUpdateMs,liveTime);
    // These attachments were emitted by the production C++ handler from MV bytes.
    await until(()=>room.state.waypoints.some(w=>w.title==='SOS Emergency'));
    const fastPoint=room.state.waypoints.find(w=>w.title==='Fast Location');
    const sosPoint=room.state.waypoints.find(w=>w.title==='SOS Emergency');
    assert(fastPoint); assert.equal(sosPoint.isSos,true);
    assert.equal(fastPoint.latitude,sosPoint.latitude);
    assert.equal(fastPoint.longitude,sosPoint.longitude);
    assert(coord.messages.some(e=>e.type==='WAYPOINT'&&e.waypoint.title==='Fast Location'));
    coord.send({type:'CMD_DELETE_WAYPOINT',...fastPoint});
    await until(()=>!room.state.waypoints.some(w=>w.title==='Fast Location'));
    assert(room.state.waypoints.some(w=>w.title==='SOS Emergency'));
    assert(room.state.messages.some(m=>m.waypoint?.title==='SOS Emergency'));
    const gatewayMeta=api.gatewayStatus(room).gatewayInfo;
    assert.equal(gatewayMeta.gatewayType,'WSS');
    assert.equal(gatewayMeta.longName,'Movetain Host');
    assert.equal(gatewayMeta.shortName,'MVT');
    assert.equal(gatewayMeta.frequency,'433.175 MHz');
    assert.equal(gatewayMeta.diagnosticStatus,true);
    const progress=rows.find(e=>e.type==='RADIO_COMMAND_STATE');assert(progress);
    gateway.send(progress);await until(()=>coord.messages.some(e=>e.type==='RADIO_COMMAND_STATE'&&e.targetNodeId===progress.targetNodeId));
    const visibleProgress=coord.messages.find(e=>e.type==='RADIO_COMMAND_STATE');
    assert.equal(visibleProgress.remainingMs,progress.remainingMs);assert.equal(visibleProgress.state,progress.state);
    coord.send({type:'CMD_SET_WSS_HOP_LIMIT',hopLimit:6});
    await until(()=>coord.messages.some(e=>e.type==='ERROR'&&e.code==='PERMISSION_DENIED'));
    owner=await connect(url);owner.send({type:'AUTH',room:'ROOM1',key:'relay-secret',token:token('test-owner')});
    await until(()=>owner.messages.some(e=>e.type==='INIT_STATE'&&e.isOwner));
    owner.send({type:'CMD_SET_WSS_HOP_LIMIT',hopLimit:6});
    await until(()=>gateway.messages.some(e=>e.type==='CMD_SET_WSS_HOP_LIMIT'));
    assert.equal(room.state.wssHopLimit,6);
    assert.equal(gateway.messages.find(e=>e.type==='CMD_SET_WSS_HOP_LIMIT').hopLimit,6);
    api.saveRoomPersistentState(room);
    assert.equal(api.loadRoomPersistentState(room.id).wssHopLimit,6);
    // Web coordinator downlink has the exact fields the firmware consumes.
    coord.send({type:'TX_LORA_CHAT',recipientId:0xaabbccdd,text:'Привет "поле"',senderCallsign:'HQ'});
    await until(()=>gateway.messages.some(e=>e.type==='CMD_TX_LORA_CHAT'));
    const chat=gateway.messages.find(e=>e.type==='CMD_TX_LORA_CHAT');
    assert.equal(chat.recipientId,0xaabbccdd); assert.equal(chat.text,'Привет "поле"'); assert(chat.id);
    gateway.send({type:'CHAT_STATUS_UPDATE',id:chat.id,status:'UNCONFIRMED'});
    await until(()=>room.state.messages.find(m=>m.id===chat.id)?.status==='UNCONFIRMED');
    gateway.send({type:'CHAT_STATUS_UPDATE',id:chat.id,status:'DELIVERED'});
    await until(()=>room.state.messages.find(m=>m.id===chat.id)?.status==='DELIVERED');
    for(const type of ['CMD_PING_NODE','CMD_POLL_ENV','CMD_POLL_AIR','CMD_TRACEROUTE']) {
      coord.send({type,targetNodeId:0xaabbccdd,nodeKey:'!aabbccdd_mv',isVirtual:true});
      await until(()=>gateway.messages.some(e=>e.type===type));
      const cmd=gateway.messages.find(e=>e.type===type); assert.equal(cmd.targetNodeId,0xaabbccdd); assert.equal(cmd.isVirtual,true);
    }
    gateway.send({type:'GATEWAY_HEARTBEAT',gatewayInfo:rows.find(e=>e.type==='AUTH_GATEWAY').gatewayInfo});
    await until(()=>gateway.messages.some(e=>e.type==='GATEWAY_HEARTBEAT_ACK'));
    assert(gateway.messages.find(e=>e.type==='GATEWAY_HEARTBEAT_ACK').timestamp>=1577836800000);
    assert(!gateway.messages.some(e=>e.type==='ERROR'),JSON.stringify(gateway.messages.filter(e=>e.type==='ERROR')));
    console.log('Real Relay WSS: auth, sanitized telemetry, live tracks, addressed chat, late ACK, diagnostic commands and clock ACK passed');
  } finally {
    gateway?.ws.terminate();coord?.ws.terminate();owner?.ws.terminate();
    for(const r of api.rooms.values()) api.cancelQueuedRoomSave(r.id);
    for(const ws of api.wss.clients)ws.terminate();
    api.wss.close();await new Promise(resolve=>api.server.close(resolve));
    // The server owns timers; exit below after disposing its isolated storage.
  }
})().then(()=>{fs.rmSync(dataDir,{recursive:true,force:true});process.exit(0)},e=>{console.error(e);fs.rmSync(dataDir,{recursive:true,force:true});process.exit(1)});
