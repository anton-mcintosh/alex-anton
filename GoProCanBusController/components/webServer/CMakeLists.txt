
idf_component_register(SRCS "webServer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "esp_http_server" "esp_netif" "esp_wifi" "softAP" "cameraControls" "ble_gopro")
