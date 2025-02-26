#ifndef SHUTTER_H
#define SHUTTER_H

#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <softAP.h>

void start_recording(httpd_req_t *req);
void stop_recording(httpd_req_t *req);

#endif

