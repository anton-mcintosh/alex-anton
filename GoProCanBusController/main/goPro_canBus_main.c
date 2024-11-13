#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <netdb.h>
#include "nvs_flash.h"
#include "esp_types.h"

#include "lwip/netdb.h"
#include "lwip/etharp.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "softAP.h"

// #define ESP_WIFI_SSID           CONFIG_ESP_WIFI_SSID
// #define ESP_WIFI_PASS           CONFIG_ESP_WIFI_PASSWORD
// #define ESP_WIFI_CHANNEL        CONFIG_ESP_WIFI_CHANNEL
// #define MAX_STA_CONN            CONFIG_ESP_MAX_STA_CONN
// #define STATIC_IP_ADDR          CONFIG_STATIC_IP_ADDR
// #define STATIC_NETMASK_ADDR     CONFIG_STATIC_NETMASK_ADDR
// #define STATIC_GW_ADDR          CONFIG_STATIC_GW_ADDR

static const char *TAG = "wifi softAP";

#define MAX_CAMERAS 4

esp_netif_t* ap_netif = NULL;

typedef struct {
    uint8_t mac[6];     // MAC address
    uint32_t ip;        // IP address (32-bit)
} CameraInfo;

uint8_t newMacAddress[6] = {0xd8, 0x96, 0x85, 0x00, 0x00, 0x00};

// Function to save an array of IP-MAC pairs in NVS
void save_camera_info_array_to_nvs(CameraInfo *pairs, int num_pairs) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(err);

    // Open NVS partition
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    // Save the IP-MAC pair array to NVS
    err = nvs_set_blob(nvs_handle, "ip_mac_pairs", pairs, sizeof(CameraInfo) * num_pairs);
    ESP_ERROR_CHECK(err);

    // Commit the changes to NVS
    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    // Close the NVS handle
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Camera Saved");
}

// Function to retrieve an array of IP-MAC pairs from NVS
void retrieve_camera_info_array_from_nvs(CameraInfo *pairs, int num_pairs) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(err);

    // Open NVS partition
    err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    // Read the IP-MAC pair array from NVS
    size_t required_size = sizeof(CameraInfo) * num_pairs;
    err = nvs_get_blob(nvs_handle, "ip_mac_pairs", pairs, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error reading IP-MAC pairs from NVS");
    }

    // Close the NVS handle
    nvs_close(nvs_handle);
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");

    wifi_init_softap();
}