#include <esp_log.h>
#include "ble_gopro.h"

static const char *TAG = "BLE_GOPRO_SHUTTER";

void start_recording_ble()
{
    ESP_LOGI(TAG, "Shutter requested!");

    if (connected_camera.shutter_char_handle == 0) {
        ESP_LOGE(TAG, "Shutter requested!");
        return;
    }
}