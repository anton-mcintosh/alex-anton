#ifndef CAMERAINFO_H
#define CAMERAINFO_H

#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <softAP.h>

void get_camera_info(httpd_req_t *req);

#endif