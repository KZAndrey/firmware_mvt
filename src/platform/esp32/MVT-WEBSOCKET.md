# WSS transport lifetime patch

`mvt_websocket_client.c` is the Apache-2.0 ESP-IDF v4.4.7 component:
https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp_websocket_client/esp_websocket_client.c

Original file SHA-256 (LF):
`8b3beca133d9bd2c293fd4e4494aadbcb42f02c6750d06a789c4d32ffd3b2a1d`.
The accompanying Apache-2.0 license is `MVT-WEBSOCKET-LICENSE`.

It replaces the SDK archive implementation only when `HAS_MVT_WSS_GATEWAY=1`.
Public headers and transport/TLS implementations remain those of the pinned SDK.

The original task polls the transport outside `client->lock`, while a failed send
closes that transport under the lock. Concurrent failures can free the same TLS
state twice. This was observed after an OTA trial boot, with the backtrace ending
in `esp_tls_internal_event_tracker_destroy` / `multi_heap_free`.

Local changes:

- Poll/read-error abort and the graceful-close poll share the existing recursive
  mutex with sends and receives. Polls use 20 ms to bound sender lock waits.
- After releasing the mutex, the higher-priority receiver blocks for one tick.
  `taskYIELD()` alone does not let a lower-priority sender acquire the mutex.
- Task final close also holds that mutex. STOPPED is published after the final
  client access, and destroy waits for STOPPED even when `run` is already false.
- Start sets `run` and clears STOPPED before scheduling the task, preventing an
  immediate stop/destroy from freeing a task that has not started yet.
- A clean-close poll timeout continues waiting instead of exiting the task.

`tests/wss/websocket_poll_test.cpp` exercises the production poll helper with
simultaneous send failure and read failure, reconnect state and graceful close.
The gateway allows 1000 ms for a TLS write instead of 20 ms. A write failure still
closes the connection and discards the partial frame; it is never treated as sent.
Send-lock contention on a still-connected client retains the queued message for
retry. It does not tear down the TLS session. Reconnect waits until the OTA worker
has exited and uses the normal retry interval after client destruction, allowing
FreeRTOS idle cleanup to reclaim the deleted task stacks before a new handshake.

## Diagnostic release 2.7.15.24101 (generation 15)

Adds numeric MVT_WSS_DIAG records before transport teardown: stage, return value,
saved errno, captured socket errno, ESP-TLS error/code/flags, uptime, Wi-Fi AP
query result, RSSI, free heap and frame counters. Zero error fields mean no error
was captured, not proof of a healthy connection. The transport error tracker is
read before abort closes it. No addresses, credentials or packet contents are
logged by this diagnostic helper. Wi-Fi state transitions and disconnect uptime /
ACK ages are logged by the gateway. Reconnect and heartbeat timings are unchanged.

2026-10-28: a hardware capture showed the small printf prints `%llu` incorrectly,
shifting the fields that follow it. Diagnostics now use 32-bit unsigned uptime
milliseconds (wraps after about 49.7 days) and include free/largest internal 8-bit
heap at the failure site. This adds observability; it does not change allocation
policy, TLS verification, retry timing or reset behavior.

## Reserved TLS record buffers (2.7.15.2910, generation 19)

The gateway reserves two internal-RAM, 16-byte-aligned record buffers in BSS,
sized from the pinned mbedTLS headers. The current SDK requests 16,717 bytes
for each input/output record in `mbedtls_ssl_setup` (also checked in the ELF).
The slots survive WebSocket teardown, so general-heap fragmentation cannot
prevent these two allocations on reconnect. They are cleared before reuse and
on release. Their memory is reserved even while the gateway is disconnected;
this trades idle free heap for a stable TLS allocation budget.

The mbedTLS platform allocator routes only matching-size allocations by the
current gateway WebSocket task to these slots. Unrelated tasks and all other
sizes retain `esp_mbedtls_mem_calloc/free`, including its memory capabilities.
The free callback recognizes reserved addresses regardless of the freeing task,
because teardown can run on the gateway control task. Atomic slot ownership
prevents aliasing; full slots fall back to the original allocator. Certificate
validation, TLS record limits, protocol and reconnect timings are unchanged.

Native tests compile the actual routing wrapper with SDK adapters. They deny
large general-heap allocations as a fragmentation model, then check 1,000
record-buffer reuse cycles, task isolation, fallback ownership, zeroing,
overflow and concurrent pool use. This is allocator fault injection, not a
measurement of ESP32 fragmentation or a substitute for on-device TLS tests.
