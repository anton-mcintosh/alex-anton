#include "camera_control.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "esp_http_client.h"
#include <inttypes.h> // Include this header for the PRId64 macro

static const char *TAG = "camera_control";

esp_err_t camera_init() {
    ESP_LOGI(TAG, "Initializing camera");
    // Camera initialization code
    return ESP_OK;
}

// esp_err_t camera_start_recording() {
//     ESP_LOGI(TAG, "Starting camera streaming");
//     // Code to start streaming

//         // Perform an HTTP POST request
//     const char *url = "http://example.com/api/upload"; // Replace with your URL
//     const char *data = "{\"key\":\"value\"}"; // Replace with your data

//     esp_err_t ret;

//     ret = camera_http_post(url, data);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to perform HTTP POST request");
//     } else {
//         ESP_LOGI(TAG, "HTTP POST request successful");
//     }

//     return ESP_OK;
// }

esp_err_t camera_stop_recording() {
    ESP_LOGI(TAG, "Stopping camera streaming");
    // Code to stop streaming
    return ESP_OK;
}

// Networking functions
esp_err_t camera_setup_udp(const char *ip, uint16_t port) {
    ESP_LOGI(TAG, "Setting up UDP connection to %s:%" PRIu16, ip, port);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    // Store the socket and address for further use
    // ...

    ESP_LOGI(TAG, "UDP connection setup successful");
    return ESP_OK;
}

esp_err_t camera_http_post(const char *url, const char *data) {
    ESP_LOGI(TAG, "Performing HTTP POST to %s", url);

    esp_http_client_config_t config = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, data, strlen(data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %" PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);

    return err;
}