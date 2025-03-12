#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "host/ble_gap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DEVICES 20

extern char discoveredDevices[MAX_DEVICES][32];
extern ble_addr_t discoveredDevicesAddr[MAX_DEVICES];
extern int numDevices;

/**
 * Start BLE discovery (scanning) for GoPro devices.
 */
int ble_gopro_discovery_start(void);

/**
 * Stop BLE discovery (scanning).
 */
int ble_gopro_discovery_stop(void);

#ifdef __cplusplus
}
#endif

#endif // DISCOVERY_H
