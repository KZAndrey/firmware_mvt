// Called between task iterations. All transport access, including poll/select,
// must use the same recursive mutex as send_with_opcode and receive.
static bool mvt_websocket_poll(esp_websocket_client_handle_t client, int *read_select)
{
    xSemaphoreTakeRecursive(client->lock, portMAX_DELAY);
    bool keep_running = true;
    if (client->run && client->state == WEBSOCKET_STATE_CONNECTED) {
        // Bound the time a sender waits behind an idle read poll.
        *read_select = esp_transport_poll_read(client->transport, 20);
        if (*read_select < 0) {
            ESP_LOGE(TAG, "Network error: esp_transport_poll_read() returned %d, errno=%d", *read_select, errno);
            esp_websocket_client_abort_connection(client);
        }
    } else if (client->run && client->state == WEBSOCKET_STATE_CLOSING &&
               (CLOSE_FRAME_SENT_BIT & xEventGroupGetBits(client->status_bits))) {
        int ret = esp_transport_ws_poll_connection_closed(client->transport, 20);
        if (ret != 0) {
            client->run = false;
            client->state = WEBSOCKET_STATE_UNKNOW;
            esp_websocket_client_dispatch_event(client, WEBSOCKET_EVENT_CLOSED, NULL, 0);
            keep_running = false;
        }
    }
    xSemaphoreGiveRecursive(client->lock);
    // Give a waiting sender a scheduling opportunity before taking the lock again.
    taskYIELD();
    return keep_running;
}
