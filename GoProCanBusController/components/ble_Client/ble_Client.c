#include "ble_Client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

static const char *TAG = "NIMBLE_CLIENT";

// Callback for BLE GAP events (e.g., connect/disconnect, discovery)
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connected to peer");
            } else {
                ESP_LOGI(TAG, "Connection failed; status=%d", event->connect.status);
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
            break;
        default:
            break;
    }
    return 0;
}

// This callback is invoked once the NimBLE host has synchronized with the controller.
static void ble_sync_callback(void) {
    ESP_LOGI(TAG, "BLE Host Sync complete. Starting scan...");

    // Set up discovery parameters
    struct ble_gap_disc_params disc_params = {
        .passive = 1,
        .itvl = 0x0010,
        .window = 0x0010,
        .filter_policy = 0,
        .limited = 0,
        .filter_duplicates = 1,
    };

    int rc = ble_gap_disc(0, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start discovery procedure; rc=%d", rc);
    }
}

// BLE Host task that runs the NimBLE event loop.
static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_client_init(void) {
    // Initialize the NimBLE port
    nimble_port_init();

    // Configure the host; set the sync callback that will be called once initialization completes.
    ble_hs_cfg.sync_cb = ble_sync_callback;

    // Start the NimBLE host task (FreeRTOS task)
    nimble_port_freertos_init(ble_host_task);
}
