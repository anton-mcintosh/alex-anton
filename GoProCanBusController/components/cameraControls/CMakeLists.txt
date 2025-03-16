idf_component_register(SRCS "cameraInfo.c" "shutter.c" "ble_shutter.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_client esp_http_server softAP ble_gopro)

