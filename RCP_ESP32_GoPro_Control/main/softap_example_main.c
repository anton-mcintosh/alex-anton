/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <netdb.h>
#include "nvs_flash.h"
#include "esp_types.h"

#include "lwip/netdb.h"
#include "lwip/etharp.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "camera_control.h"

#define EXAMPLE_WIFI_SSID             "HERO-RC-000000"
#define EXAMPLE_WIFI_PASS             ""
#define EXAMPLE_ESP_WIFI_CHANNEL      1
#define EXAMPLE_MAX_STA_CONN          4
#define EXAMPLE_STATIC_IP_ADDR        "10.71.79.1"
#define EXAMPLE_STATIC_NETMASK_ADDR   "255.255.255.0"
#define EXAMPLE_STATIC_GW_ADDR        "10.71.79.1"

#define BUTTON_GPIO 4

#define MAX_CAMERAS                   4

esp_netif_t* ap_netif = NULL;
static QueueHandle_t button_event_queue = NULL;

typedef struct {
    uint8_t mac[6];     // MAC address
    uint32_t ip;        // IP address (32-bit)
} CameraInfo;

void button_init() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_up_en = 1; // Enable pull-up if your button is connected to ground
    io_conf.pull_down_en = 0; // Disable pull-down
    gpio_config(&io_conf);
}




uint8_t newMacAddress[6] = {0xd8, 0x96, 0x85, 0x00, 0x00, 0x00};

static const char *TAG = "wifi softAP";

// Function to save an array of IP-MAC pairs in NVS
void save_camera_info_array_to_nvs(CameraInfo *pairs, int num_pairs) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(err);

    // Open NVS partition
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    // Save the IP-MAC pair array to NVS
    err = nvs_set_blob(nvs_handle, "ip_mac_pairs", pairs, sizeof(CameraInfo) * num_pairs);
    ESP_ERROR_CHECK(err);

    // Commit the changes to NVS
    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    // Close the NVS handle
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Camera Saved");
}

// Function to retrieve an array of IP-MAC pairs from NVS
void retrieve_camera_info_array_from_nvs(CameraInfo *pairs, int num_pairs) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(err);

    // Open NVS partition
    err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    // Read the IP-MAC pair array from NVS
    size_t required_size = sizeof(CameraInfo) * num_pairs;
    err = nvs_get_blob(nvs_handle, "ip_mac_pairs", pairs, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error reading IP-MAC pairs from NVS");
    }

    // Close the NVS handle
    nvs_close(nvs_handle);
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);

        CameraInfo camera_array[MAX_CAMERAS];
        uint32_t ip = (10) | (71) | (79) | (2);
        memcpy(camera_array[0].mac, event->mac, sizeof(event->mac));
        camera_array[0].ip = ip;
        save_camera_info_array_to_nvs(camera_array, MAX_CAMERAS);

    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void dhcp_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    switch (event_id)
    {
    case IP_EVENT_AP_STAIPASSIGNED:
        ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
        ESP_LOGI("IP_EVENT", "A device was assigned this IP:" IPSTR, IP2STR(&event->ip));
        break;

    default:
        break;
    }
}

void wifi_init_softap(void)
{
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
                                                NULL, //&server,
                                                NULL)); 
 
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
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
    esp_netif_dhcps_start(ap_netif);
    /* STATIC IP ENDS*/
 
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
   
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL); 
}

#define PORT 8384 // Port of the UDP server
#define HOST_IP_ADDR "10.0.10.71.79.2" // IP address of the UDP server

static void IRAM_ATTR button_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(button_event_queue, &gpio_num, NULL);
}

void udp_client_task(void *pvParameters) {
    char message[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x53, 0x48, 0x02};
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE("UDP", "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI("UDP", "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);

    uint32_t io_num;
    while (1)
    {
        if (xQueueReceive(button_event_queue, &io_num, portMAX_DELAY))
        {
            ESP_LOGI("UDP", "Button pressed, sending packet");
            int err = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0)
            {
                ESP_LOGE("UDP", "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI("UDP", "Message sent");
        }
    }

    if (sock != -1)
    {
        ESP_LOGI("UDP", "Shutting down socket");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}

void button_task(void* arg) {
    int last_state = 0;
    while (1) {
        int state = gpio_get_level(BUTTON_GPIO);
        if (state != last_state) {
            last_state = state;
            if (state == 1) { // Assuming the button is active-high
                ESP_LOGI("Button", "Button pressed!");
                // camera_start_recording();
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Debouncing delay
    }
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");

    wifi_init_softap();

    // CameraInfo retrieved_array[MAX_CAMERAS];
    // retrieve_camera_info_array_from_nvs(retrieved_array, MAX_CAMERAS);
    // ESP_LOGI("NVS", "Saved camera is "MACSTR" and IP is what",
    //         MAC2STR(retrieved_array[0].mac));

    button_init();

    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    button_event_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, (void*) BUTTON_GPIO);

// Set up UDP connection to the camera
    const char *ip = "10.71.79.2"; // Replace with your camera's IP address
    uint16_t port = 8484; // Replace with the appropriate port number

    camera_setup_udp(ip,port);

    // // Start UDP client task
    // xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);

}


