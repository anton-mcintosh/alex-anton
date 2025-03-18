#include <esp_log.h>
#include "ble_gopro.h"

static const char *TAG = "BLE_GOPRO_SHUTTER";

void start_recording_ble()
{
    ESP_LOGI(TAG, "Shutter requested!");

        // Create a two-byte command array:
        uint8_t shutter_command[4] = { 3, 1, 1, 1 };

        // Send the command with the updated two-byte length.
        gopro_write_command(shutter_command, sizeof(shutter_command));
}