# Reconnection regression coverage (2.7.15.1810)

2026-10-24: `websocket_receive_test.cpp` now executes the actual receive helper.
The original helper failed the bounded test on a stalled partial frame. The
helper now rejects zero progress inside a frame, checks stop between reads and
limits the full frame to 30 seconds (individual transport reads remain 10s).
This prevents an indefinitely drip-fed frame from keeping the transport mutex
and blocking stop/reconnect. Empty control frames and valid chunked frames are
still accepted. This reproduction does not establish the cause of a historical
hardware outage without logs from that outage.

The production gateway now separates a connection attempt from the 15-second
retry delay. A live TLS attempt has a 30-second lifecycle budget; the configured
transport timeout is 10 seconds. DISCONNECTED/CLOSED marks the client for
retirement by the gateway thread, not by the SDK callback. Teardown keeps the
normal delay before allocating another TLS client and waits for the OTA worker
to release its resources.

After authentication, a valid server heartbeat ACK refreshes an application
liveness clock. After 75 seconds without an ACK the gateway retires the client,
even if the transport still looks connected. Authentication success starts the
grace period. This does not override rejected credentials, replacement or admin
blocks, and it does not reboot or reset configuration.

The native gateway test executes 50 alternating CLOSED/DISCONNECTED cycles,
checks that each client is destroyed, the retry pause is respected, AUTH is
sent again, and no stale frame is kept. It also checks a handshake longer than
the retry interval, the handshake deadline, refreshed ACK liveness and loss of
ACK followed by a new client. Normal radio tests synthesize server heartbeat
ACKs; the reconnection cases deliberately do not.

Run tests/wss/run.py with the documented Zig and relay arguments. Host adapters
check lifecycle logic and API calls, not real TLS heap fragmentation or Wi-Fi.
The existing WebSocket poll test separately checks production mutex scheduling.

Hardware acceptance still needs repeated server restart / network interruption
on a canary, without resetting the board. Record MVT WSS connect/retire/unhealthy
lines and heap/largest-block measurements. The read-only USB capture made before
this change did not reproduce the user's persistent RETRY state, so these are
recovery fixes, not proof that the original symptom was caused by a memory leak.

## Failed OTA retry investigation (2026-10-28)

A reported transfer from 2.7.15.24101 to 2.7.15.2710.1 left the display at
WSS RETRY until a manual restart. A second attempt completed. The server's
current job confirms `updated` for generation 17; the first job was replaced,
and no USB log of that first failure was captured. Seeing the previous version
alone does not establish a bootloader rollback. The exact trigger is unproven.

The investigation found retained allocations across failed transfers: the
one-element OTA chunk queue and manifest-only strings, plus the receive string's
capacity after WSS teardown. Failed OTA cleanup now runs on the gateway task
only after the worker publishes completion. It retains job identifiers/error
for status reporting. WSS releases frame capacity after joining the receiver.
This reduces memory pressure for the next TLS handshake; it does not establish
that memory pressure caused the reported incident or guarantee hardware recovery.

Regression coverage checks queue lifetime before/after worker completion,
repeat cleanup and subsequent attempts, cancellation before startup/during
signature verification/while awaiting chunks/after partial flash writes,
receive timeout, flash write failure, signature failure and task allocation
failure. Fifty reconnect cycles also check that a large frame allocation is
released. Host adapters do not reproduce ESP32 heap fragmentation or TLS timing.
A hardware reproduction must capture USB output from before the OTA command,
including MVT OTA failure/cleanup, WSS connect heap/largest and MVT_WSS_DIAG.

## Captured TLS allocation failure on 2.7.15.2810 (2026-10-28)

The user confirmed 2.7.15.2810 on the device. A passive COM5 capture (DTR/RTS
disabled, no commands or reset) recorded nine `mbedtls_ssl_setup returned
-0x7F00` failures. The pinned SDK defines this as MBEDTLS_ERR_SSL_ALLOC_FAILED.
Gateway logs reported Wi-Fi connected (`wifi=3`), about 99,960 free heap bytes
and a 32,756-byte largest block before attempts, with retries about 15 seconds
apart. These values are before transport and TLS allocations, not the failed
allocator's requested size or heap layout. They do not establish a memory leak
or identify the allocation retaining memory.

At 23:25:22 UTC+5 the same capture recorded `MVT WSS authenticated`, followed
by `MVT snapshot NodeDB entries=14`; uptime continued without resetting. The
room API subsequently confirmed gatewayOnline and WSS. Recovery happened without
an agent-issued reset or firmware change. This is evidence of delayed recovery,
not a fix or a guarantee that every future failure will recover.

The diagnostic formatter also printed `uptime_ms=lu` and shifted subsequent
arguments. Fields after this unsupported `%llu` must not be used from that
capture. The formatter now uses 32-bit unsigned milliseconds and additionally
reports free/largest internal 8-bit heap at the error site, matching the pinned
SDK's TLS allocation capability. This diagnostic change does not fix TLS memory
pressure. Hardware acceptance still requires repeated reconnects and allocation
measurements; host tests cannot validate fragmentation.

The native gateway suite now injects the pre-AUTH DISCONNECTED event produced
by a failed TLS connection: nine consecutive attempts, then a separate roughly
one-hour (240 attempts) scenario. It checks teardown counts, no AUTH/data before
connection, the full retry pause, unchanged room credentials, and recovery on
the same gateway object with buffered delivery exactly once. Init and task-start
allocation failures are injected separately. These scenarios test recovery
control flow; they do not allocate real ESP32 TLS buffers.

The USB diagnostic candidate additionally registers an allocator failure hook
filtered to the WebSocket task. The hook only stores atomic numeric values; it
does not allocate, log or inspect heap lists. MVT_WSS_ALLOC outside the callback
reports the attempt's failure count and last requested size/capabilities. It is
an observer, not fault injection or a memory allocator replacement.

## USB hardware acceptance, 2026-10-29 (UTC+5)

Heltec V3 / ESP32-S3, existing Wi-Fi/room/radio configuration preserved. The
active app0 slot was identified from the partition table and CRC-checked OTA
selection records; an 8 MiB backup was taken before application-only writes.

Eight actual Relay process restarts on the observer-only build recovered in
about 18–19 seconds each, including three after warming the gateway cache with
13 real NodeDB entries. They did not reproduce the original allocation failure.
The ToPhone packet queue uses a static pool on this target; its full-queue
warning is not evidence that it consumed the missing TLS heap.

A temporary USB-only test build then reserved both 16,717-byte TLS records.
After a server restart it deliberately rejected record allocation on attempts
2 and 3. Both produced the captured -0x7F00 / ESP-TLS 32791 / code 32512 error
while Wi-Fi remained connected. Attempt 4 authenticated without a device reset,
using both reserved buffers while large record allocations from the general
allocator were disabled. Two more Relay restarts authenticated in about
18–19 seconds using the same reserve, without further errors or device resets.
Connected free heap with the warmed cache was about 37–38 KiB.

This proves recovery under controlled allocation failure and reuse of the
reserve on actual TLS handshakes. It does not identify the exact allocation
that failed in the original uninstrumented 2810 incident, or prove that all
possible memory-pressure failures are eliminated. Other TLS allocations still
use the general heap; numeric failure observations remain in the release.
Artificial failure and cache-warmup code is removed from the public build.

The clean generation-19 image (2.7.15.2910) was written to the same app0 slot
and authenticated at 00:40:14 without changing the saved configuration. Its
binary was checked for the exact release marker and absence of test markers.

An additional remote gateway still on 2810 did not return promptly after the
00:30:15 server restart. Server history confirms `server_shutdown` and no later
authenticated connection as of 00:40; there is no UART capture from that device.
This is evidence that the old release can fail to recover promptly after a
restart, but does not prove the remote device's internal failure is also TLS
allocation. Further shared-server restart testing was stopped. Future destructive
transport testing should use an isolated endpoint or a single-device fault proxy.
