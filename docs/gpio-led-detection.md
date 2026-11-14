# GPIO_v2 — Detection Sensor с подтверждением сохранения

Рабочая версия и ветка: **GPIO_v2**. Только GPIO LED, nRF52840 Pro Micro DIY TCXO (`nrf52_promicro_diy_tcxo`). Не прошивка WSS/Heltec; WiFi-Gateway/GTW не обновляется.

## Зачем

Старый текстовый отчёт не содержал времени события, номера GPIO и подтверждения приёма порталом. Потеря радиопакета могла остаться незамеченной. Новая версия добавляет ограниченную повторную доставку. Это не гарантия доставки при отключённой сети и не система аварийной защиты.

## Настройка

Основной публичный канал остаётся прежним. Нужен включённый канал **gpio** (регистр имени неважен), с явным PSK 16 или 32 байта; этот же ключ на MQTT-шлюзе и в комнате. Слот канала не фиксирован. Без подходящего канала отправка отключается, перехода на публичный канал нет.
Настройки Detection Sensor по-прежнему задаются родным приложением: GPIO22, оба перехода с активным HIGH, минимум 15 секунд, периодический отчёт 900 секунд. Значения не зашиты в бинарник. GPIO22 нельзя одновременно использовать в Remote Hardware. GPIO8/24 — выходы LED, GPIO20 — вход TOUCH. Remote Hardware не изменён.

## Доставка и ограничения

- Событие передаётся через gpio на порту 10 в двоичном формате MDS2. Портал сохраняет его и только после успешного сохранения отправляет адресный MDS2 ACK.
- ACK связан с ID исходной ноды, случайной сессией загрузки, номером события и его MAC. Чужой, изменённый и старый ACK не удаляет другое событие.
- Очередь только в RAM: четыре события. Максимум три передачи каждого события; повтор через 10 минут, срок жизни 30 минут. Между любыми передачами минимум 15 секунд. При переполнении удаляется самое старое, счётчик потерь включается в новые отчёты. Перезапуск теряет очередь.
- У каждого повтора новый mesh packet ID, но прежний ID события. Портал не дублирует журнал и уведомление; повтор может получить новый ACK (не чаще раза в 15 секунд).
- Журнал: 24 часа, максимум 2000 записей на комнату. Последнее состояние хранится отдельно. Восемь предыдущих сессий сохраняются для защиты от запоздавших повторов; это ограниченная дедупликация, не вечная защита от replay.
- Время события берётся только из синхронизированных часов ноды. Если его нет, портал показывает **Event time unavailable**. Received — время приёма порталом, не время срабатывания.
- Если периодический отчёт не пришёл за два интервала плюс 10 минут, интерфейс показывает No recent reports. При неизвестном/нулевом интервале — Freshness unknown. Старый Inactive не доказывает отсутствие движения сейчас.
- Минимальный интервал штатного модуля может пропускать короткие переходы. Очередь не устраняет это ограничение.

## Безопасность и совместимость

HMAC-SHA256 (первые 16 байт) добавлен внутри существующего шифрованного канала. Любой обладатель gpio PSK входит в общую область доверия; это не PKI и не доказательство личности конкретного портала. HMAC защищает MDS2 от изменения посторонними, но не исправляет протокол Remote Hardware.
Промежуточные ноды ретранслируют пакеты без модуля Detection Sensor. MQTT-шлюзу нужны gpio и uplink/downlink, не включение самого датчика. Родное приложение может настраивать модуль, но **не отображает новые бинарные MDS2 события как штатный текстовый журнал**. Старые текстовые сообщения портал продолжает принимать без application ACK и времени события. Новая обработка сейчас относится к линии MQTT; Map Reporting не добавлялся.
Другие платы без MOVETAIN_DETECTION_GPIO_CHANNEL сохраняют прежнее поведение.

## Формат MDS2

Little endian. Событие: magic MDS2 (4), type=1 (1), source ID (4), boot stream (8), sequence (4), UTC seconds или 0 (4), период отчёта в секундах (4), dropped (2), pin (1), level (1), detection/state (1), длина имени (1), имя (1–20), MAC (16).
ACK: magic (4), type=2 (1), portal ID (4), target ID (4), stream (8), sequence (4), MAC исходного события (16), MAC ACK (16). Ключ HMAC — полный gpio PSK.

## Проверки, сборка, установка

`python tests/test_sensor_delivery.py <C++ compiler> [args]` компилирует реальные методы прошивки и библиотеку Crypto. Проверяет канал/ключ, подделанные ACK, ограничение повторов, переполнение, истечение, переполнение millis и сверяет HMAC независимым Python hashlib.
`python tests/test_detection_gpio_channel.py` — совместимый вход в тот же тест (требуются аргументы компилятора).
`pio run -e nrf52_promicro_diy_tcxo`; локально detection-local.ini содержит пути пакетов и не публикуется. `version.properties: custom = GPIO_v2`.
Перед установкой сохранить конфигурацию, проверить ID !b7418dc7 и модель. Использовать UF2 без factory erase. После — прочитать версию и сравнить RF-настройки, каналы/отпечатки ключей и модули. Не коммитить UF2/ZIP, логи, ключи и резервные копии. Бинарник публикуется release asset, исходники/тесты/документация — в Git.


## USB diagnostic logging and event clocks (2026-11-15)

Live COM27 retest: logging off gave two complete configuration snapshots without protobuf errors. Logging on gave 2, 2 and 3 decode errors, only one config section and no module sections despite config-complete. Original settings were restored byte-for-byte. `StreamAPI::emitLogRecord` reused `PhoneAPI::fromRadioScratch` and `txBuf` during response construction (including logging after encoding). The fix gives each log its own message and encoded buffer, and bounds newline trimming by actual stored length. The host regression executes the actual logging method with response sentinels, empty/long/newline messages and encoder failure. A new on-device retest is required after flashing this build.

All sensor trigger modes call the same `queueEvent`, which stores `getValidTime(RTCQualityFromNet)`. Unknown time remains zero; retries retain the original timestamp. Do not invent event time from receipt time. The USB wizard provides an explicit standard admin `set_time_only` command using computer UTC seconds, separately from configuration writes. ACK confirms command handling, not clock accuracy or persistence across power loss. Computer, node and server clocks can differ. Live HIGH test after synchronization displayed event timestamps; local telemetry was within two seconds of computer time. The computer was about 3–4 seconds ahead of server HTTP Date, explaining Event time one second later than Received. Historical records are not rewritten.

Live verification after UF2 installation (SHA-256 `0781694ecc3beaf485d4131b1761e191420bc04e32f1e84f2d4189e83ec1edd4`): all three debug-enabled sessions decoded without errors and returned all 10 config sections, 13 module sections and 8 channels. The sessions received 81, 155 and 83 log records respectively. Debug was verified enabled during each session, then restored off; configuration, modules, channels and security matched the pre-test snapshots byte-for-byte. The version remains GPIO_v2. This records installation on the test node; it does not claim publication of a new signed download catalog.
