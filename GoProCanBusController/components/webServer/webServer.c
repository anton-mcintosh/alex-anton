#include "webServer.h"
#include "softAP.h"
#include "shutter.h"
#include "cameraInfo.h"
#include "discovery.h"
// #include "pairing.h"
#include "ble_gopro.h"
#include "cJSON.h"

static const char *TAG = "webserver";

// Static function for handling HTTP GET requests
static esp_err_t get_handler(httpd_req_t *req)
{
    // Open the HTML file
    FILE *file = fopen("/spiffs/index.html", "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open index.html");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File not found");
        return ESP_FAIL;
    }

    // Read and send file content in chunks (chunked encoding)
    char buffer[128];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(file);
    
    // Indicate the end of the response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Button clicked! Event triggered. URI: %s", req->uri);

    if (strcmp(req->uri, "/start") == 0) {
      start_recording(req);
    }
    else if (strcmp(req->uri, "/stop") == 0) {
      stop_recording(req);
    }
    else if (strcmp(req->uri, "/info") == 0) {
      get_camera_info(req);
    }
    else if (strcmp(req->uri, "/startscan") == 0) {
      ESP_LOGI(TAG, "Start scan requested");
      ble_gopro_scan();
      httpd_resp_send(req, "BLE scan initiated.", HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
    else if (strcmp(req->uri, "/stopscan") == 0) {
      ESP_LOGI(TAG, "Stop scan requested");
      // ble_gopro_discovery_stop();
      httpd_resp_send(req, "BLE scan cancelled.", HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
    else if (strcmp(req->uri, "/pair") == 0) {
      char buf[100];
      int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
      if(ret > 0) {
          buf[ret] = '\0';
          // Parse JSON (you might use cJSON here) to extract the device name.
          // For simplicity, assume buf contains: {"device": "GoPro 5390"}
          cJSON *root = cJSON_Parse(buf);
          cJSON *deviceItem = cJSON_GetObjectItem(root, "device");
          if (deviceItem && cJSON_IsString(deviceItem)) {
              // ble_gopro_pair(deviceItem->valuestring);
              httpd_resp_send(req, "Pairing initiated", HTTPD_RESP_USE_STRLEN);
          } else {
              httpd_resp_send(req, "Invalid JSON", HTTPD_RESP_USE_STRLEN);
          }
          cJSON_Delete(root);
      } else {
          httpd_resp_send(req, "Failed to read request", HTTPD_RESP_USE_STRLEN);
      }
  }
    else {
      ESP_LOGE(TAG, "Invalid URI: %s", req->uri);
      httpd_resp_send(req, "Invalid URI", HTTPD_RESP_USE_STRLEN);
      return ESP_FAIL;
    }
    httpd_resp_send(req, "Action triggered on ESP32.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t list_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateArray();
  for (int i = 0; i < numDevices; i++) {
      cJSON_AddItemToArray(root, cJSON_CreateString(discoveredDevices[i]));
  }
  char *json_str = cJSON_Print(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, strlen(json_str));
  cJSON_Delete(root);
  free(json_str);
  return ESP_OK;
}

void server_initiation(void)
{
    httpd_handle_t server_handle = NULL;
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();

    // Start the HTTP Server
    esp_err_t ret = httpd_start(&server_handle, &server_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return;
    }

    // Register the GET handler
    httpd_uri_t uri_get = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = get_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_get);

    // Register the GET handler for camera info if needed
    httpd_uri_t uri_info = {
      .uri = "/info",
      .method = HTTP_GET,
      .handler = post_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_info);

    // Register the POST handler for start recording
    httpd_uri_t uri_start = {
        .uri = "/start",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_start);
    
    // Register the POST handler for stop recording
    httpd_uri_t uri_stop = {
      .uri = "/stop",
      .method = HTTP_POST,
      .handler = post_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_stop);
    
    // Register the POST handler for starting BLE scanning
    httpd_uri_t uri_startscan = {
        .uri = "/startscan",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_startscan);

      // Register the POST handler for starting BLE scanning
      httpd_uri_t uri_stopscan = {
        .uri = "/stopscan",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_stopscan);

      // Register the POST handler for starting BLE pairing
      httpd_uri_t uri_pair = {
        .uri = "/pair",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_pair);

    httpd_uri_t uri_list = {
      .uri = "/list",
      .method = HTTP_GET,
      .handler = list_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_list);

    ESP_LOGI(TAG, "HTTP server started and handlers registered");
}
