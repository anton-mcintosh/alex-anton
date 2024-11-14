#include "shutter.h"
#include "softAP.h"

static const char *TAG = "shutter";

void start_recording(httpd_req_t *req) {
  esp_err_t err;
  esp_http_client_config_t config = {
    .url = "http://10.71.79.2/gp/gpControl/command/shutter?p=1",
    .method = HTTP_METHOD_GET,
    .timeout_ms = 5000,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "HTTP GET request sent!");
    httpd_resp_send(req, "Recording started!", HTTPD_RESP_USE_STRLEN);
  }
  else {
    ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    httpd_resp_send(req, "Recording failed to start!", HTTPD_RESP_USE_STRLEN);
  }
esp_http_client_cleanup(client);
}

void stop_recording(httpd_req_t *req) {
  esp_err_t err;
  esp_http_client_config_t config = {
    .url = "http://10.71.79.2/gp/gpControl/command/shutter?p=0",
    .method = HTTP_METHOD_GET,
    .timeout_ms = 5000,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "HTTP GET request sent!");
    httpd_resp_send(req, "Recording stopped!", HTTPD_RESP_USE_STRLEN);
  }
  else {
    ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    httpd_resp_send(req, "Recording failed to stop!", HTTPD_RESP_USE_STRLEN);
  }
esp_http_client_cleanup(client);
}

