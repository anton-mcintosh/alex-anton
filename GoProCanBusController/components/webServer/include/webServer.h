#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <esp_http_server.h>
#include <esp_err.h>
#include "shutter.h"

// Function to initialize the HTTP server
void server_initiation(void);

#endif // WEBSERVER_H
