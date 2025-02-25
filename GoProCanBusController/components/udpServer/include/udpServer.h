#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "esp_log.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UDP_PORT 8383
#define UDP_REMOTE_PORT 8484
#define UDP_REMOTE_IP "10.71.79.2"

void udp_server_init(void);
void udp_send_message(const char *message);

#endif
