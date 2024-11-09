#ifndef CAMERA_CONTROL_H
#define CAMERA_CONTROL_H

#include "esp_err.h"

// Initialize the camera
esp_err_t camera_init();

// Other camera control functions
esp_err_t camera_start_recording();
esp_err_t camera_stop_recording();

// Networking functions
esp_err_t camera_setup_udp(const char *ip, uint16_t port);
// esp_err_t camera_http_post(const char *url, const char *data);

#endif // CAMERA_CONTROL_H