// gatt.c
#include "ble_gopro.h" // Your public header; it should include the basic NimBLE and peer headers.
#include "esp_log.h"
#include "nimble/ble.h"
#include "host/ble_hs.h" // Provides declarations for ble_gattc_write()
#include "host/ble_gatt.h"
#include "peer.h" // For peer_chr_find_uuid() and peer definitions.
#include <string.h>

// for memory heap debugging
#include "esp_heap_caps.h"

static const char *TAG = "GATT";

static int
ble_on_write(uint16_t conn_handle, const struct ble_gatt_error *error,
                 struct ble_gatt_attr *attr, void *arg)
{
    if (error->status != 0)
    {
        MODLOG_DFLT(ERROR, "Error: Write procedure failed; status=%d conn_handle=%d\n",
                    error->status, conn_handle);
        /* Optionally, terminate the connection if the write failed */
        // ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        // return error->status;
    }

    MODLOG_DFLT(INFO, "Write procedure succeeded; conn_handle=%d attr_handle=%d\n",
                conn_handle, attr->handle);

    /* Implement post-write logic here, such as initiating a read or subscribe operation */

    return 0;
}

/**
 * Application callback.  Called when the read of the ANS Supported New Alert
 * Category characteristic has completed.
 */
static int
ble_on_read(uint16_t conn_handle,
            const struct ble_gatt_error *error,
            struct ble_gatt_attr *attr,
            void *arg)
{
    if (error->status != 0)
    {
        MODLOG_DFLT(ERROR, "Error: Read procedure failed; status=%d conn_handle=%d\n",
                    error->status, conn_handle);
        /* Optionally, terminate the connection if the write failed */
        // ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        // return error->status;
    }

    MODLOG_DFLT(INFO, "Read procedure succeeded; conn_handle=%d attr_handle=%d\n",
                conn_handle, attr->handle);

    /* Implement post-write logic here, such as initiating a read or subscribe operation */

    return 0;
}

void subscribe_to_characteristics(const struct peer *peer)
{

    MODLOG_DFLT(INFO, "subscribe_to_characterists connection_handle: %d\n", connected_camera.connection_handle);
    struct peer_svc *svc;
    struct peer_chr *chr;
    struct peer_dsc *dsc;
    int rc;

    MODLOG_DFLT(INFO, "Starting subscribing to discovered services!");

    // Iterate over all discovered services
    SLIST_FOREACH(svc, &peer->svcs, next)
    {
        char svc_uuid_str[BLE_UUID_STR_LEN];
        ble_uuid_to_str((const ble_uuid_t *)&svc->svc.uuid, svc_uuid_str);
        MODLOG_DFLT(INFO, "Found service with UUID: %s, handle range: %d - %d\n",
                    svc_uuid_str, svc->svc.start_handle, svc->svc.end_handle);
        ble_uuid_t *desired_uuid = BLE_UUID16_DECLARE(GOPRO_SERVICE_UUID);
        if (ble_uuid_cmp((const ble_uuid_t *)&svc->svc.uuid, desired_uuid) == 0)
        {
            MODLOG_DFLT(INFO, "GoPro Control and Query Service found!  Subscribing to the characteristics");
            // Iterate over all characteristics in the current service
            SLIST_FOREACH(chr, &svc->chrs, next)
            {
                char chr_uuid_str[BLE_UUID_STR_LEN];
                ble_uuid_to_str((const ble_uuid_t *)&chr->chr.uuid, chr_uuid_str);
                MODLOG_DFLT(INFO, "  Found characteristic with UUID: %s, properties: 0x%x, value handle: %d\n",
                            chr_uuid_str, chr->chr.properties, chr->chr.val_handle);

                // Iterate over all descriptors in the current characteristic
                SLIST_FOREACH(dsc, &chr->dscs, next)
                {
                    char dsc_uuid_str[BLE_UUID_STR_LEN];
                    ble_uuid_to_str((const ble_uuid_t *)&dsc->dsc.uuid, dsc_uuid_str);
                    MODLOG_DFLT(INFO, "    Found descriptor with UUID: %s, handle: %d\n",
                                dsc_uuid_str, dsc->dsc.handle);

                    // Check if the descriptor is the CCCD
                    if (ble_uuid_cmp(&dsc->dsc.uuid.u, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) == 0)
                    {
                        uint16_t ccc_value = 0;

                        // Determine if notifications or indications are supported
                        if (chr->chr.properties & BLE_GATT_CHR_PROP_NOTIFY)
                        {
                            MODLOG_DFLT(INFO, "Characteristic has a notify property!");
                            ccc_value |= BLE_GATT_CHR_PROP_NOTIFY;
                            // }
                            // if (chr->chr.properties & BLE_GATT_CHR_PROP_INDICATE)
                            // {
                            //     MODLOG_DFLT(INFO, "Characteristic has an indicate property!");
                            //     ccc_value |= BLE_GATT_CHR_PROP_INDICATE;
                            // }

                            // if (ccc_value != 0)
                            // {

                            // Write to the CCCD to enable notifications
                            rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                                                      // &ccc_value, sizeof(ccc_value), ble_on_write, NULL);
                                                      &ccc_value, sizeof(ccc_value), NULL, NULL);
                            if (rc != 0)
                            {
                                MODLOG_DFLT(ERROR, "Failed to write CCCD; rc=%d\n", rc);
                            }
                            else
                            {
                                MODLOG_DFLT(INFO, "Subscribed to characteristic with handle=%d\n", chr->chr.val_handle);
                            }
                        }
                    }
                }
            }
        }
    }
}

void assign_command_handle(const struct peer *peer)
{
    MODLOG_DFLT(INFO, "Searching for the Command UUID");
    struct peer_svc *svc;
    struct peer_chr *chr;
    ble_uuid_t *chr_UUID;

    char uuid_str[BLE_UUID_STR_LEN];  // Buffer for UUID string conversion

    // Iterate over discovered services.
    SLIST_FOREACH(svc, &peer->svcs, next)
    {

        // Within this service, iterate over its characteristics.
        SLIST_FOREACH(chr, &svc->chrs, next)
        {
            chr_UUID = (ble_uuid_t *)&chr->chr.uuid;
            // Compare the characteristic UUID with the GoPro command UUID.
            if (ble_uuid_cmp(chr_UUID, gopro_command_uuid) == 0)
            {
                connected_camera.command_handle = chr->chr.val_handle;
                MODLOG_DFLT(INFO, "Assigned command handle: %d\n", connected_camera.command_handle);
                return; // Exit once the correct handle is assigned.
            }
        }
    }
    MODLOG_DFLT(ERROR, "Command characteristic not found!\n");
}


void gopro_write_command(const uint8_t *data, uint16_t data_len) {
    ESP_LOGI(TAG, "Writing command to camera");
    ESP_LOGI(TAG, "Connected camera connection_handle: %d", connected_camera.connection_handle);
    ESP_LOGI(TAG, "Command handle: %d", connected_camera.command_handle);
    ESP_LOGI(TAG, "Data length: %d", data_len);

    // Build a hex string from the data buffer.
    char data_hex[256] = {0};  // Adjust size if needed
    char *ptr = data_hex;
    for (int i = 0; i < data_len; i++) {
        sprintf(ptr, "%02X ", data[i]);
        ptr += 3;
    }
    ESP_LOGI(TAG, "Data: %s", data_hex);

    int rc = ble_gattc_write_no_rsp_flat(
        connected_camera.connection_handle,
        connected_camera.command_handle,
        data,
        data_len
    );

    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Failed to write command; rc=%d\n", rc);
    }
}