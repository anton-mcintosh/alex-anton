#include "udpServer.h"

static const char *TAG = "UDP_SERVER";
static int udp_socket;
static struct sockaddr_in remote_addr;

void udp_server_task(void *pvParameters) {
    struct sockaddr_in server_addr;
    char rx_buffer[128];
    socklen_t addr_len = sizeof(remote_addr);

    // Create UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        vTaskDelete(NULL);
    }

    // Bind socket to port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(UDP_PORT);

    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(udp_socket);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "UDP Server listening on port %d", UDP_PORT);

    while (1) {
        int len = recvfrom(udp_socket, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&remote_addr, &addr_len);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }

        rx_buffer[len] = '\0';
        ESP_LOGI(TAG, "Received %d bytes from %s: %s", len, inet_ntoa(remote_addr.sin_addr), rx_buffer);
    }

    close(udp_socket);
    vTaskDelete(NULL);
}

void udp_send_message(const char *message) {
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(UDP_REMOTE_PORT);
    inet_pton(AF_INET, UDP_REMOTE_IP, &remote_addr.sin_addr);

    sendto(udp_socket, message, 14, 0,  // 14 bytes as per your specified packet
           (struct sockaddr *)&remote_addr, sizeof(remote_addr));

    ESP_LOGI(TAG, "Sent UDP packet (14 bytes)");
}

void udp_server_init(void) {
    xTaskCreate(udp_server_task, "udp_server_task", 4096, NULL, 5, NULL);
}
