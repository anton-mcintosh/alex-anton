#include "ble_gopro.h"
#include <stdio.h>
#include <assert.h>

static const char *TAG = "BLE_GOPRO";

gopro_camera_t connected_camera;

void ble_store_config_init(void);

// Global storage for discovered devices.
char discoveredDevices[MAX_DEVICES][32];
ble_addr_t discoveredDevicesAddr[MAX_DEVICES];
int numDevices = 0;

/*
 * Reset callback for the BLE host.
 */
static void blecent_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

/*
 * Sync callback for the BLE host.
 */
static void blecent_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
}

/*
 * The BLE host task.
 */
void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/*
 * Initiates the GAP discovery procedure.
 */
void ble_gopro_scan(void)
{
    ESP_LOGI(TAG, "Initializing scanning...");
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }
    disc_params.filter_duplicates = 1;
    disc_params.passive = 1;
    disc_params.itvl = 0x0010;
    disc_params.window = 0x0010;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, 30 * 1000, &disc_params,
                      blecent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n", rc);
    }
}

/*
 * Initializes the BLE host.
 */
void ble_gopro_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE...");
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return;
    }

    ble_hs_cfg.reset_cb = blecent_on_reset;
    ble_hs_cfg.sync_cb = blecent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;

    int rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    rc = ble_svc_gap_device_name_set("nimble-blegopro");
    assert(rc == 0);

    ble_store_config_init();
    nimble_port_freertos_init(ble_host_task);
}