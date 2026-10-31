# OTA receive-buffer ownership

During a hardware OTA failure, cJSON could not allocate a 2,734-byte string:
10,652 bytes were free, but the largest contiguous block was only 2,548 bytes.
The completed WebSocket frame was copied into the consumer queue while the
receiver retained its original payload allocation. The JSON parser then needed
another allocation for the encoded firmware chunk.

The receiver now transfers its completed string to the queue. The consumer
owns it until command processing returns, including when a command retires
the connection and drains the remaining queue. Queue rejection and disconnect
cleanup delete the queued objects. Frame bounds, ordering and NUL checks remain.

Regression coverage in gateway_test.cpp verifies that a fragmented 5-KiB
command retains the original buffer in the queue, releases the receiver's
payload capacity, and executes once. Existing queue-overflow, disconnect,
reconnect, OTA cancellation and real Relay integration cases also run.
The capacity regression failed against the previous implementation before the
fix; the full host suite passed after the ownership change.

This reduces peak memory use; it does not guarantee every future allocation
will succeed. Separately, the earlier firmware was observed aborting after a
raw WebSocket write returned zero. No errno or TLS error established the
underlying cause of that write failure. Partial/raw write failures must still
retire the connection because bytes may already have been transmitted.

The release retains the 3110 sender's pre-write backpressure handling and
five-second send budget, signed OTA verification, power checks and rollback.
