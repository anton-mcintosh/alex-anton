#include "softAP.h"
#include "esp_http_client.h"

// Static variables if needed
static const char *TAG = "softAP";
static esp_netif_t *ap_netif = NULL;

/*Function to trigger recording on gopro hero4*/
void start_recording(httpd_req_t *req) {
  esp_err_t err;
    // gopro ip: 10.71.79.4
    // trigger shutter API call: http://10.71.79.4/gp/gpControl/command/shutter?p=1
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
    // gopro ip: 10.71.79.2
    // trigger shutter API call: http://10.71.79.2/gp/gpControl/command/shutter?p=0
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

// Event handler implementations
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data) {
    // Handle Wi-Fi events here
}
void dhcp_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data) {
    // Handle DHCP events here
}

// Function implementation
void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                    ESP_EVENT_ANY_ID,
                                                    &wifi_event_handler,
                                                    NULL,
                                                    NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                    ESP_EVENT_ANY_ID,
                                                    &dhcp_event_handler,
                                                    NULL,
                                                    NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    // Set the IP
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));

    esp_netif_ip_info_t ip_info;

    IP4_ADDR(&ip_info.ip, 10, 71, 79, 1);
    IP4_ADDR(&ip_info.gw, 10, 71, 79, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    // Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}
