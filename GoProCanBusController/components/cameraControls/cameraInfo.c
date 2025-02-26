#include "camerainfo.h"
#include "softAP.h"

static const char *TAG = "camera_info";

void get_camera_info(httpd_req_t *req) {
    esp_err_t err;
    char buffer[1024];  // Adjust size depending on expected JSON length
    int content_length;

    esp_http_client_config_t config = {
        .url = "http://10.71.79.2/gp/gpControl/status",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        content_length = esp_http_client_get_content_length(client);
        if (content_length > sizeof(buffer) - 1) {
            content_length = sizeof(buffer) - 1;  // Prevent buffer overflow
        }
        esp_http_client_read(client, buffer, content_length);
        buffer[content_length] = '\0';  // Null-terminate the string

        ESP_LOGI(TAG, "Camera Info JSON: %s", buffer);
        httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);  // Optional: send JSON back to web client
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        httpd_resp_send(req, "Failed to fetch camera info", HTTPD_RESP_USE_STRLEN);
    }

    esp_http_client_cleanup(client);
}
