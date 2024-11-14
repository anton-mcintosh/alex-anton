#ifndef SOFTAP_H
#define SOFTAP_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <esp_http_server.h>
#include <esp_http_client.h>
#include <netdb.h>
#include "nvs_flash.h"
#include "esp_types.h"

#include "lwip/netdb.h"
#include "lwip/etharp.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define ESP_WIFI_SSID           CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS           CONFIG_ESP_WIFI_PASSWORD
#define ESP_WIFI_CHANNEL        CONFIG_ESP_WIFI_CHANNEL
#define MAX_STA_CONN            CONFIG_ESP_MAX_STA_CONN
#define STATIC_IP_ADDR          CONFIG_STATIC_IP_ADDR
#define STATIC_NETMASK_ADDR     CONFIG_STATIC_NETMASK_ADDR
#define STATIC_GW_ADDR          CONFIG_STATIC_GW_ADDR

// Externally accessible functions
void wifi_init_softap(void);

// If these event handlers are needed externally, declare them here
// Otherwise, you can keep them static in softAP.c
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data);

void dhcp_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data);

#endif // SOFTAP_H
