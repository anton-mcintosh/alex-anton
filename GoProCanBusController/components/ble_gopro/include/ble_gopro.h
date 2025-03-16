#ifndef BLE_GOPRO_H
#define BLE_GOPRO_H

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "esp_bt.h"

// BLE utility includes
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"

// Other module includes
#include "peer.h"
#include "misc.h"

#define BLEGOPRO_QUERY_UUID              0xFEA6

// Maximum devices discovered (adjust as needed)
#define MAX_DEVICES 20

#ifdef __cplusplus
extern "C" {
#endif

// Camera structure
typedef struct {
    uint16_t conn_id;                  // Connection identifier
    uint16_t shutter_char_handle;      // Handle for the shutter command characteristic
    ble_addr_t camera_address;          // The camera's unique Bluetooth Device Address

    // You can add more fields for other characteristics if needed
} gopro_camera_t;

static gopro_camera_t connected_camera;

// Global variables (if they need to be accessed from other modules)
extern char discoveredDevices[MAX_DEVICES][32];
extern ble_addr_t discoveredDevicesAddr[MAX_DEVICES];
extern int numDevices;

// Public API function prototypes
void ble_gopro_init(void);
void ble_gopro_scan(void);
void ble_host_task(void *param);

// GAP event handler used by the scan; defined in ble_gopro_gap.c
int blecent_gap_event(struct ble_gap_event *event, void *arg);

// GATT subscription
void subscribe_to_characteristics(const struct peer *peer);

#ifdef __cplusplus
}
#endif

#endif // BLE_GOPRO_H
