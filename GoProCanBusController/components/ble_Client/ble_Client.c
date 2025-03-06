#include "ble_Client.h"
#include "gopro_protocol.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h> // for sprintf

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "gap.h"

static const char *TAG = "BLE_CLIENT";

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);

/* Private functions */
/*
 *  Stack event callback functions
 *      - on_stack_reset is called when host resets BLE stack due to errors
 *      - on_stack_sync is called when host has synced with controller
 */
static void on_stack_reset(int reason)
{
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void)
{
    /* On stack sync, do advertising initialization */
    adv_init();
}

static void nimble_host_config_init(void)
{
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Store host configuration */
    ble_store_config_init();
}

static void nimble_host_task(void *param)
{
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}

// // Buffer size for BLE address strings: "xx:xx:xx:xx:xx:xx" + null terminator.
// #define BLE_ADDR_STR_LEN 18

// // Forward declaration of ble_gap_event callback.
// static int ble_gap_event(struct ble_gap_event *event, void *arg);

// // Helper: Convert a BLE address to a human-readable string.
// static void ble_addr_to_str(const ble_addr_t *addr, char *str) {
//     sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
//             addr->val[0], addr->val[1], addr->val[2],
//             addr->val[3], addr->val[4], addr->val[5]);
// }

// // Helper: Parse advertisement data for a 16-bit UUID matching 0xFEA6.
// static int gopro_adv_filter(const uint8_t *adv_data, uint8_t adv_data_len) {
//     uint8_t pos = 0;
//     while (pos < adv_data_len) {
//         uint8_t field_len = adv_data[pos];
//         if (field_len == 0) break;
//         if (pos + field_len >= adv_data_len) break; // Prevent overflow.
//         uint8_t field_type = adv_data[pos + 1];
//         // Use the updated macros for incomplete/complete 16-bit UUIDs.
//         if (field_type == BLE_HS_ADV_TYPE_INCOMP_UUIDS16 ||
//             field_type == BLE_HS_ADV_TYPE_COMP_UUIDS16) {
//             // Each UUID is 2 bytes in little-endian.
//             for (uint8_t i = pos + 2; i < pos + field_len + 1; i += 2) {
//                 if (i + 1 >= adv_data_len) break;
//                 uint16_t uuid = adv_data[i] | (adv_data[i + 1] << 8);
//                 if (uuid == 0xFEA6) {
//                     return 1; // Found the GoPro service.
//                 }
//             }
//         }
//         pos += field_len + 1;
//     }
//     return 0;
// }

// // Helper task to restart scanning after a delay.
// static void restart_scan_task(void *param) {
//     // Delay for 500 ms to let the system settle.
//     vTaskDelay(pdMS_TO_TICKS(500));
//     struct ble_gap_disc_params disc_params = {
//         .passive = 1,
//         .itvl = 0x0010,
//         .window = 0x0010,
//         .filter_policy = 0,
//         .limited = 0,
//         .filter_duplicates = 1,
//     };
//     int rc = ble_gap_disc(0, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Error restarting scan; rc=%d", rc);
//     }
//     vTaskDelete(NULL);
// }

// // GAP event callback: handles discovery, connection, and disconnection.
// static int ble_gap_event(struct ble_gap_event *event, void *arg) {
//     switch (event->type) {
//         case BLE_GAP_EVENT_DISC: {
//             struct ble_gap_disc_desc *disc = &event->disc;
//             if (gopro_adv_filter(disc->data, disc->length_data)) {
//                 char addr_str[BLE_ADDR_STR_LEN];
//                 ble_addr_to_str(&disc->addr, addr_str);
//                 ESP_LOGI(TAG, "Found GoPro device: %s", addr_str);
//                 // Cancel scanning before initiating connection.
//                 int rc = ble_gap_disc_cancel();
//                 if (rc != 0) {
//                     ESP_LOGE(TAG, "Failed to cancel scanning; rc=%d", rc);
//                 }
//                 rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &disc->addr,
//                                      30000, NULL, ble_gap_event, NULL);
//                 if (rc != 0) {
//                     ESP_LOGE(TAG, "Failed to initiate connection; rc=%d", rc);
//                 }
//             }
//             break;
//         }
//         case BLE_GAP_EVENT_CONNECT: {
//             if (event->connect.status == 0) {
//                 struct ble_gap_conn_desc conn_desc;
//                 int rc = ble_gap_conn_find(event->connect.conn_handle, &conn_desc);
//                 if (rc == 0) {
//                     char addr_str[BLE_ADDR_STR_LEN];
//                     // Use the peer identity address.
//                     ble_addr_to_str(&conn_desc.peer_id_addr, addr_str);
//                     ESP_LOGI(TAG, "Successfully paired with device: %s", addr_str);
//                 } else {
//                     ESP_LOGI(TAG, "Successfully paired; but failed to get connection descriptor");
//                 }
//             } else {
//                 ESP_LOGE(TAG, "Connection failed; status=%d", event->connect.status);
//             }
//             break;
//         }
//         case BLE_GAP_EVENT_DISCONNECT: {
//             ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
//             // Restart scanning after disconnect using a dedicated task.
//             xTaskCreate(restart_scan_task, "restart_scan", 2048, NULL, tskIDLE_PRIORITY, NULL);
//             break;
//         }
//         default:
//             break;
//     }
//     return 0;
// }

// // Called when the BLE host syncs with the controller; starts scanning.
// static void ble_sync_callback(void) {
//     ESP_LOGI(TAG, "BLE host sync complete. Starting scan...");
//     struct ble_gap_disc_params disc_params = {
//         .passive = 1,
//         .itvl = 0x0010,
//         .window = 0x0010,
//         .filter_policy = 0,
//         .limited = 0,
//         .filter_duplicates = 1,
//     };
//     int rc = ble_gap_disc(0, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Error initiating discovery; rc=%d", rc);
//     }
// }

// // The BLE host task: runs the NimBLE event loop.
// static void ble_host_task(void *param) {
//     ESP_LOGI(TAG, "BLE host task started");
//     nimble_port_run();
//     nimble_port_freertos_deinit();
// }

// Public API: Initialize the BLE client.
void ble_client_init(void) {

    /* NimBLE stack initialization */
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    /* GAP service initialization */
    int rc = gap_init();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();

    /* Start NimBLE host task thread and return */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);

}
