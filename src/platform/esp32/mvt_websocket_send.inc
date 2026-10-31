static int esp_websocket_client_send_with_opcode(esp_websocket_client_handle_t client, ws_transport_opcodes_t opcode, const uint8_t *data, int len, TickType_t timeout)
{
    int need_write = len;
    int wlen = 0, widx = 0;
    int ret = ESP_FAIL;
    uint32_t current_opcode = opcode;
    const uint32_t started = (uint32_t)_tick_get_ms();

    if (client == NULL || len < 0 ||
        (opcode != WS_TRANSPORT_OPCODES_CLOSE && (data == NULL || len <= 0))) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_FAIL;
    }

    if (xSemaphoreTakeRecursive(client->lock, timeout) != pdPASS) {
        ESP_LOGE(TAG, "Could not lock ws-client within %d timeout", timeout);
        return ESP_FAIL;
    }

    if (!esp_websocket_client_is_connected(client)) {
        ESP_LOGE(TAG, "Websocket client is not connected");
        goto unlock_and_return;
    }

    if (client->transport == NULL) {
        ESP_LOGE(TAG, "Invalid transport");
        goto unlock_and_return;
    }
    while (widx < len || current_opcode) {  // allow for sending "current_opcode" only message with len==0
        // Only this preflight is known to precede ALL bytes of the frame.
        // A zero from send_raw is ambiguous: its header may already be sent.
        const uint32_t elapsed = (uint32_t)(_tick_get_ms() - started);
        const uint32_t budget = timeout * portTICK_PERIOD_MS;
        int remaining = timeout == portMAX_DELAY ? -1 : elapsed < budget ? (int)(budget - elapsed) : 0;
        errno = 0;
        const int ready = remaining ? esp_transport_poll_write(client->control_transport, remaining) : 0;
        const int poll_errno = errno;
        if (ready <= 0) {
            // Retrying a whole message after a completed non-FIN fragment
            // would violate continuation ordering, even with no new header.
            if (widx || (ready < 0 && poll_errno != EINTR && poll_errno != EAGAIN)) {
                mvt_ws_diagnostic(client, "write_preflight", ready, poll_errno);
                esp_websocket_client_abort_connection(client);
            }
            ret = ESP_FAIL;
            goto unlock_and_return;
        }
        if (timeout != portMAX_DELAY) {
            const uint32_t spent = (uint32_t)(_tick_get_ms() - started);
            if (spent >= budget) {
                if (widx) {
                    mvt_ws_diagnostic(client, "write_preflight", 0, 0);
                    esp_websocket_client_abort_connection(client);
                }
                ret = ESP_FAIL;
                goto unlock_and_return;
            }
            remaining = (int)(budget - spent);
        }
        if (need_write > client->buffer_size) {
            need_write = client->buffer_size;
        } else {
            current_opcode |= WS_TRANSPORT_OPCODES_FIN;
        }
        memcpy(client->tx_buffer, data + widx, need_write);
        // send with ws specific way and specific opcode
        errno = 0;
        wlen = esp_transport_ws_send_raw(client->transport, current_opcode, (char *)client->tx_buffer, need_write, remaining);
        if (wlen != need_write) {
            ret = wlen;
            mvt_ws_diagnostic(client, "write", ret, errno);
            ESP_LOGE(TAG, "Network error: esp_transport_write() returned %d, errno=%d", ret, errno);
            esp_websocket_client_abort_connection(client);
            goto unlock_and_return;
        }
        current_opcode = 0;
        widx += wlen;
        need_write = len - widx;

    }
    ret = widx;
unlock_and_return:
    xSemaphoreGiveRecursive(client->lock);
    return ret;
}
