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

#define BLEGOPRO_QUERY_UUID     0xFEA6
#define GOPRO_SERVICE_UUID      0xFEA6


// Maximum devices discovered (adjust as needed)
#define MAX_DEVICES 20

#ifdef __cplusplus
extern "C" {
#endif

static const ble_uuid_t *gopro_command_uuid = BLE_UUID128_DECLARE(
    0x1b, 0xc5, 0xd5, 0xa5,
    0x02, 0x00, 0x46, 0x90,
    0xe3, 0x11, 0x8d, 0xaa,
    0x72, 0x00, 0xf9, 0xb5
);

// Camera structure
typedef struct {
    uint16_t connection_handle;    // Connection handler
    uint16_t command_handle;       // Handle for the shutter command characteristic
    ble_addr_t camera_address;     // The camera's unique Bluetooth Device Address
} gopro_camera_t;

extern gopro_camera_t connected_camera;

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

// GATT functions
void subscribe_to_characteristics(const struct peer *peer);
void assign_command_handle(const struct peer *peer);
void gopro_write_command(const uint8_t *data, uint16_t data_len);

#ifdef __cplusplus
}
#endif

#endif // BLE_GOPRO_H
