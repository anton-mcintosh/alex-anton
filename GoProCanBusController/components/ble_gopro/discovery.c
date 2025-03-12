#include "discovery.h"
#include "esp_log.h"
#include "nimble/ble.h"
#include "host/ble_gap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

#ifndef BLE_ADDR_STR_LEN
#define BLE_ADDR_STR_LEN 18
#endif

#ifndef ble_addr_to_str
static inline char *ble_addr_to_str(const ble_addr_t *addr, char *str) {
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr->val[0],
            addr->val[1],
            addr->val[2],
            addr->val[3],
            addr->val[4],
            addr->val[5]);
    return str;
}
#endif

static const char *TAG = "BLE_DISCOVERY";

// Global storage for discovered devices.
char discoveredDevices[MAX_DEVICES][32];
ble_addr_t discoveredDevicesAddr[MAX_DEVICES];
int numDevices = 0;

static TaskHandle_t discovery_task_handle = NULL;
static bool discovery_running = false;
static volatile bool scan_active = false;
static int ble_gap_event_discovery(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            const struct ble_gap_disc_desc *disc = &event->disc;
            char addr_str[BLE_ADDR_STR_LEN];
            ble_addr_to_str(&disc->addr, addr_str);
            
            bool is_gopro = false;
            char device_name[31] = {0};
            const uint8_t *adv_data = disc->data;  // now declared as const
            uint8_t adv_length = disc->length_data;
            int pos = 0;
            while (pos < adv_length) {
                uint8_t field_len = adv_data[pos];
                if (field_len == 0)
                    break;
                uint8_t field_type = adv_data[pos + 1];
                // Check for Service Data (AD type 0x16) and look for GoPro service UUID 0xFEA6 (little-endian)
                if (field_type == 0x16 && field_len >= 3) {
                    uint16_t svc_uuid = adv_data[pos + 2] | (adv_data[pos + 3] << 8);
                    if (svc_uuid == 0xFEA6) {
                        is_gopro = true;
                    }
                }
                // Check for Complete Local Name (AD type 0x09)
                else if (field_type == 0x09) {
                    int name_len = field_len - 1;
                    if (name_len > 30)
                        name_len = 30;
                    memcpy(device_name, &adv_data[pos + 2], name_len);
                    device_name[name_len] = '\0';
                }
                pos += field_len + 1;
            }
            if (is_gopro && device_name[0] != '\0') {
                ESP_LOGI(TAG, "Discovered GoPro: %s, RSSI: %d, Name: %s", addr_str, disc->rssi, device_name);
                bool alreadyFound = false;
                for (int i = 0; i < numDevices; i++) {
                    if (strcmp(discoveredDevices[i], device_name) == 0) {
                        alreadyFound = true;
                        break;
                    }
                }
                if (!alreadyFound && numDevices < MAX_DEVICES) {
                    strncpy(discoveredDevices[numDevices], device_name, 31);
                    discoveredDevices[numDevices][31] = '\0';
                    discoveredDevicesAddr[numDevices] = disc->addr;
                    numDevices++;
                    ESP_LOGI(TAG, "Added device: %s", device_name);
                }
            }
            break;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(TAG, "Discovery complete; reason: %d", event->disc_complete.reason);
            scan_active = false;
            break;
        default:
            break;
    }
    return 0;
}


static void discovery_task(void *param) {
    ESP_LOGI(TAG, "BLE discovery task started");
    discovery_running = true;
    numDevices = 0; // reset device list at start
    while (discovery_running) {
        if (!scan_active) {
            scan_active = true;
            struct ble_gap_disc_params disc_params;
            memset(&disc_params, 0, sizeof(disc_params));
            disc_params.passive = 0;
            disc_params.filter_duplicates = 1;
            disc_params.itvl = 0x0010;
            disc_params.window = 0x0010;
            disc_params.limited = 0;
            disc_params.filter_policy = 0;
            int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30 * 1000, &disc_params, ble_gap_event_discovery, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Error starting BLE scan; rc=%d", rc);
                scan_active = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "BLE discovery task ending");
    scan_active = false;
    discovery_task_handle = NULL;
    vTaskDelete(NULL);
}

int ble_gopro_discovery_start(void) {
    numDevices = 0;
    if (discovery_task_handle != NULL) {
        ESP_LOGW(TAG, "BLE discovery task already running");
        return 0;
    }
    BaseType_t result = xTaskCreate(discovery_task, "ble_discovery_task", 4096, NULL, 5, &discovery_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create BLE discovery task");
        return -1;
    }
    return 0;
}

int ble_gopro_discovery_stop(void) {
    discovery_running = false;
    int rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to cancel BLE scan; rc=%d", rc);
    }
    scan_active = false;
    return rc;
}
