const WS_URL = 'wss://movetain.app/ws';
const ROOM_ID = 'IDA302741E';
const RELAY_KEY = 'mvt_5acd7a0e5b38114b';

console.log('=== Запуск эмулятора MVT WSS Gateway (Heltec Stick V3) ===');
console.log(`Сервер: ${WS_URL}`);
console.log(`Комната: ${ROOM_ID}`);
console.log(`Relay Key: ${RELAY_KEY.substring(0, 6)}...`);

const ws = new WebSocket(WS_URL);

let heartbeatTimer = null;

ws.onopen = () => {
    console.log('\n[WS] Соединение установлено! Отправка AUTH_GATEWAY...');

    const authPayload = {
        type: 'AUTH_GATEWAY',
        room: ROOM_ID,
        relayKey: RELAY_KEY,
        adminKey: RELAY_KEY,
        source: 'Heltec Stick V3 Standalone',
        timestamp: Date.now(),
        gatewayInfo: {
            radioConnected: true,
            nodeHex: '68ef1234',
            nodeId: 1760498228,
            nodeName: 'Heltec_V3_GW',
            phoneBattery: 95,
            radioBattery: 95,
            voltage: 4.15,
            transport: 'Direct WSS Gateway (WiFi)',
            freeHeap: 178240,
            firmwareVersion: '2.7.15-mvt'
        }
    };

    ws.send(JSON.stringify(authPayload));
};

ws.onmessage = (event) => {
    try {
        const msg = JSON.parse(event.data);
        console.log(`\n[ВХОДЯЩИЙ ПАКЕТ] Тип: ${msg.type}`);

        if (msg.type === 'AUTH_GATEWAY_SUCCESS') {
            console.log('>>> [УСПЕХ] ШЛЮЗ АВТОРИЗОВАН В КОМНАТЕ! <<<');
            console.log(`Комната: ${msg.roomName || msg.room}`);

            // 1. Отправляем тестового участника LoRa в эфире
            setTimeout(() => {
                console.log('\n[LORA -> WSS] Передаем обнаруженный узел в поле: LoRa_Scout_1...');
                const nodeUpdate = {
                    type: 'NODE_UPDATE',
                    nodeKey: '!68ef9999',
                    node: {
                        nodeKey: '!68ef9999',
                        nodeId: 1760532889,
                        nodeHex: '68ef9999',
                        callsign: 'LoRa_Scout_1',
                        lat: 55.751244,
                        lon: 37.618423,
                        alt: 180,
                        battery: 88,
                        radioBattery: 88,
                        voltage: 3.95,
                        lastUpdateMs: Date.now()
                    }
                };
                ws.send(JSON.stringify(nodeUpdate));

                // 2. Отправляем приветственное радиосообщение
                const chatMsg = {
                    type: 'CHAT_MESSAGE',
                    message: {
                        id: Math.floor(Math.random() * 1000000),
                        senderId: 1760532889,
                        senderCallsign: 'LoRa_Scout_1',
                        recipientId: 0xFFFFFFFF,
                        text: 'Тест связи: Heltec Wireless Stick V3 онлайн по прямому WSS!',
                        timestampMs: Date.now(),
                        isSos: false,
                        isOutgoing: false,
                        hops: 1,
                        status: 'DELIVERED'
                    }
                };
                ws.send(JSON.stringify(chatMsg));
                // 3. Отправляем Target Point (целевую точку)
                const targetWp = {
                    type: 'WAYPOINT',
                    waypoint: {
                        title: 'Target Alpha',
                        comment: 'Целевая точка полевой группы',
                        latitude: 55.753500,
                        longitude: 37.622000,
                        altitude: 185.0,
                        timestampSec: Math.floor(Date.now() / 1000),
                        isRadio: true,
                        isTarget: true
                    }
                };
                ws.send(JSON.stringify(targetWp));
                console.log('[LORA -> WSS] Target Point (Target Alpha) отправлена в портал!');
            }, 1000);

            // 4. Запускаем периодический хартбит шлюза (каждые 15 сек)
            heartbeatTimer = setInterval(() => {
                const hb = {
                    type: 'GATEWAY_HEARTBEAT',
                    timestamp: Date.now(),
                    gatewayInfo: {
                        radioConnected: true,
                        nodeHex: '68ef1234',
                        nodeId: 1760498228,
                        radioBattery: 95,
                        voltage: 4.15,
                        freeHeap: 178240,
                        minFreeHeap: 165320
                    }
                };
                ws.send(JSON.stringify(hb));
                console.log(`[GATEWAY_HEARTBEAT] Отправлен хартбит (FreeHeap: 178KB, Bat: 95%)`);
            }, 15000);
        }

        if (msg.type === 'CMD_TX_LORA_WAYPOINT') {
            const wp = msg.waypoint || msg;
            console.log('\n======================================================');
            console.log(`[TARGET POINT ИЗ ПОРТАЛА] Координатор поставил точку: "${wp.title || 'Target'}"`);
            console.log(`Координаты: ${wp.latitude}, ${wp.longitude}`);
            console.log(`Флаг isTarget: ${wp.isTarget || false}`);
            console.log('>>> Heltec Stick V3 транслирует эту Target Point в эфир LoRa! <<<');
            console.log('======================================================\n');
        }

        if (msg.type === 'WAYPOINT_DELETED') {
            console.log(`\n[ТОЧКА УДАЛЕНА В ПОРТАЛЕ] Удалена точка на координатах ${msg.latitude}, ${msg.longitude}`);
        }

        if (msg.type === 'CMD_TX_LORA_CHAT') {
            console.log('\n======================================================');
            console.log(`[КОМАНДА ИЗ ПОРТАЛА] Координатор написал в чат: "${msg.text}"`);
            console.log(`Получатель: 0x${(msg.recipientId || 0xFFFFFFFF).toString(16)}`);
            console.log('>>> Heltec Stick V3 передает это сообщение в эфир LoRa! <<<');
            console.log('======================================================\n');

            // Имитируем радио-ответ от полевого узла
            setTimeout(() => {
                const reply = {
                    type: 'CHAT_MESSAGE',
                    message: {
                        id: Math.floor(Math.random() * 1000000),
                        senderId: 1760532889,
                        senderCallsign: 'LoRa_Scout_1',
                        recipientId: 0xFFFFFFFF,
                        text: `Принято в поле по LoRa: "${msg.text}"`,
                        timestampMs: Date.now(),
                        isSos: false,
                        isOutgoing: false,
                        hops: 1,
                        status: 'DELIVERED'
                    }
                };
                ws.send(JSON.stringify(reply));
                console.log('[LORA -> WSS] Отправлен автоответ полевой группы в портал!');
            }, 1500);
        }

        if (msg.type === 'ERROR') {
            console.error(`[ОШИБКА ОТ СЕРВЕРА] ${msg.code || ''}: ${msg.message}`);
        }
    } catch (e) {
        console.error('[PARSE ERROR]', e.message);
    }
};

ws.onerror = (err) => {
    console.error('[WS ERROR]', err.message || err);
};

ws.onclose = (event) => {
    console.log(`[WS CLOSED] Код: ${event.code}, Причина: ${event.reason}`);
    if (heartbeatTimer) clearInterval(heartbeatTimer);
};
