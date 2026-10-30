// Called under client->lock, between complete received frames. Use a complete
// masked frame on the cached TLS/TCP parent: the SDK raw-send API returns zero
// both for a successfully sent empty control frame and for a poll timeout.
static bool mvt_websocket_send_control(esp_websocket_client_handle_t client, int opcode,
                                       const char *payload, int length)
{
    if (length < 0 || length > 125 || (length && !payload)) return false;
    unsigned char wire[6 + 125];
    wire[0] = (unsigned char)(opcode | WS_TRANSPORT_OPCODES_FIN);
    wire[1] = (unsigned char)(0x80 | length);
    esp_fill_random(wire + 2, 4);
    for (int i = 0; i < length; ++i) wire[6 + i] = (unsigned char)payload[i] ^ wire[2 + i % 4];
    const uint32_t started = _tick_get_ms();
    int offset = 0;
    while (offset < length + 6) {
        int remaining = client->config->network_timeout_ms - (int)(uint32_t)(_tick_get_ms() - started);
        if (remaining <= 0) { mvt_ws_diagnostic(client, "control_deadline", -1, 0); return false; }
        errno = 0;
        int written = esp_transport_write(client->control_transport, (const char *)wire + offset, length + 6 - offset, remaining);
        if (written <= 0 || written > length + 6 - offset) {
            mvt_ws_diagnostic(client, opcode == WS_TRANSPORT_OPCODES_PING ? "ping_write" :
                opcode == WS_TRANSPORT_OPCODES_PONG ? "pong_write" : "close_write", written, errno);
            return false;
        }
        offset += written;
    }
    return true;
}
