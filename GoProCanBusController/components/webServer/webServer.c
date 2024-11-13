#include "webServer.h"
#include <esp_log.h>

static const char *TAG = "webserver";

// Static function for handling HTTP GET requests
static esp_err_t get_handler(httpd_req_t *req)
{
    const char *response_message = "<!DOCTYPE HTML><html><head>\
                                    <title>Interactive Page</title>\
                                    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
                                    </head><body>\
                                    <h1>ESP32 HTTP Server</h1>\
                                    <form action=\"/trigger\" method=\"post\">\
                                    <button type=\"submit\">Trigger Event</button>\
                                    </form><br>\
                                    </body></html>";
    httpd_resp_send(req, response_message, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t post_handler(httpd_req_t *req)
{
    ESP_LOGI("webserver", "Button clicked! Event triggered.");

    // Send a response back to the client
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

    // Register the POST handler
    httpd_uri_t uri_post = {
        .uri = "/trigger",
        .method = HTTP_POST,
        .handler = post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_handle, &uri_post);

    ESP_LOGI(TAG, "HTTP server started and handlers registered");
}