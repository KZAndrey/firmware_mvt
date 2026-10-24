static esp_err_t esp_websocket_client_recv(esp_websocket_client_handle_t client)
{
    int rlen;
    const uint32_t started = _tick_get_ms();
    client->payload_offset = 0;
    do {
        // A peer can drip-feed a frame forever while holding the transport lock.
        // Bound the entire frame and let stop() retire the worker without reboot.
        const uint32_t elapsed = (uint32_t)(_tick_get_ms() - started);
        if (!client->run || elapsed >= 30000) {
            ESP_LOGE(TAG, "Receive stopped or frame deadline exceeded");
            return ESP_FAIL;
        }
        const int remaining = (int)(30000 - elapsed);
        const int timeout = client->config->network_timeout_ms < remaining ? client->config->network_timeout_ms : remaining;
        rlen = esp_transport_read(client->transport, client->rx_buffer, client->buffer_size, timeout);
        if (rlen < 0 || !client->run || (uint32_t)(_tick_get_ms() - started) >= 30000) {
            ESP_LOGE(TAG, "Error read data");
            return ESP_FAIL;
        }
        client->payload_len = esp_transport_ws_get_read_payload_len(client->transport);
        client->last_opcode = esp_transport_ws_get_read_opcode(client->transport);

        if (rlen == 0 && client->last_opcode == WS_TRANSPORT_OPCODES_NONE ) {
            ESP_LOGV(TAG, "esp_transport_read timeouts");
            return client->payload_offset == 0 ? ESP_OK : ESP_FAIL;
        }

        if (rlen == 0 && client->payload_offset < client->payload_len) {
            ESP_LOGE(TAG, "Incomplete frame made no progress");
            return ESP_FAIL;
        }

        esp_websocket_client_dispatch_event(client, WEBSOCKET_EVENT_DATA, client->rx_buffer, rlen);

        client->payload_offset += rlen;
    } while (client->payload_offset < client->payload_len);

    // if a PING message received -> send out the PONG, this will not work for PING messages with payload longer than buffer len
    if (client->last_opcode == WS_TRANSPORT_OPCODES_PING) {
        const char *data = (client->payload_len == 0) ? NULL : client->rx_buffer;
        ESP_LOGD(TAG, "Sending PONG with payload len=%d", client->payload_len);
        esp_transport_ws_send_raw(client->transport, WS_TRANSPORT_OPCODES_PONG | WS_TRANSPORT_OPCODES_FIN, data, client->payload_len,
                                  client->config->network_timeout_ms);
    } else if (client->last_opcode == WS_TRANSPORT_OPCODES_PONG) {
        client->wait_for_pong_resp = false;
    } else if (client->last_opcode == WS_TRANSPORT_OPCODES_CLOSE) {
        ESP_LOGD(TAG, "Received close frame");
        client->state = WEBSOCKET_STATE_CLOSING;
    }

    return ESP_OK;
}
