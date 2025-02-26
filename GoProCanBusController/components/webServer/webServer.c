#include "webServer.h"
#include "softAP.h"
#include "shutter.h"
#include "cameraInfo.h"

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
    ESP_LOGI("webserver", "Button clicked! Event triggered.");

    if (strcmp(req->uri, "/start") == 0) {
      start_recording(req);
    }
    else if (strcmp(req->uri, "/stop") == 0) {
      stop_recording(req);
    }
    else if (strcmp(req->uri, "/info") == 0) {
      get_camera_info(req);
    }
    else {
      ESP_LOGE(TAG, "Invalid URI: %s", req->uri);
      httpd_resp_send(req, "Invalid URI", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send(req, "Button clicked! Event triggered on ESP32.", HTTPD_RESP_USE_STRLEN);
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

    // Register the GET handler
    httpd_uri_t uri_info = {
      .uri = "/info",
      .method = HTTP_GET,
      .handler = post_handler,
      .user_ctx = NULL
  };
  httpd_register_uri_handler(server_handle, &uri_info);

    // Register the POST handler
    httpd_uri_t uri_start = {
        .uri = "/start",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL
    };
    httpd_uri_t uri_stop = {
      .uri = "/stop",
      .method = HTTP_POST,
      .handler = post_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_start);
    httpd_register_uri_handler(server_handle, &uri_stop);

    ESP_LOGI(TAG, "HTTP server started and handlers registered");
}
