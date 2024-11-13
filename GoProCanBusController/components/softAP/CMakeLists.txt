idf_component_register(SRCS "softAP.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_server esp_http_client esp_wifi esp_event esp_netif nvs_flash lwip)
