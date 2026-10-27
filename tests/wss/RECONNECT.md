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
