// gatt.c
#include "ble_gopro.h" // Your public header; it should include the basic NimBLE and peer headers.
#include "esp_log.h"
#include "nimble/ble.h"
#include "host/ble_hs.h" // Provides declarations for ble_gattc_write()
#include "host/ble_gatt.h"
#include "peer.h" // For peer_chr_find_uuid() and peer definitions.
#include <string.h>

// static const char *TAG = "GATT";

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
// static int
// ble_on_read(uint16_t conn_handle,
//             const struct ble_gatt_error *error,
//             struct ble_gatt_attr *attr,
//             void *arg)
// {
//     if (error->status != 0)
//     {
//         MODLOG_DFLT(ERROR, "Error: Read procedure failed; status=%d conn_handle=%d\n",
//                     error->status, conn_handle);
//         /* Optionally, terminate the connection if the write failed */
//         // ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
//         // return error->status;
//     }

//     MODLOG_DFLT(INFO, "Read procedure succeeded; conn_handle=%d attr_handle=%d\n",
//                 conn_handle, attr->handle);

//     /* Implement post-write logic here, such as initiating a read or subscribe operation */

//     return 0;
// }

void subscribe_to_characteristics(const struct peer *peer)
{
    struct peer_svc *svc;
    struct peer_chr *chr;
    struct peer_dsc *dsc;
    int rc;

    // Iterate over all discovered services
    SLIST_FOREACH(svc, &peer->svcs, next)
    {
        // Iterate over all characteristics in the current service
        SLIST_FOREACH(chr, &svc->chrs, next)
        {
            // Iterate over all descriptors in the current characteristic
            SLIST_FOREACH(dsc, &chr->dscs, next)
            {
                // Check if the descriptor is the CCCD
                if (ble_uuid_cmp(&dsc->dsc.uuid.u, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) == 0)
                {
                    uint16_t ccc_value = 0;

                    // Determine if notifications or indications are supported
                    if (chr->chr.properties & BLE_GATT_CHR_PROP_NOTIFY)
                    {
                        ccc_value |= BLE_GATT_CHR_PROP_NOTIFY;
                    }
                    if (chr->chr.properties & BLE_GATT_CHR_PROP_INDICATE)
                    {
                        ccc_value |= BLE_GATT_CHR_PROP_INDICATE;
                    }

                    if (ccc_value != 0)
                    {
                        // Write to the CCCD to enable notifications/indications
                        rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                                                  &ccc_value, sizeof(ccc_value), ble_on_write, NULL);
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

// UUID for the control characteristic
static const ble_uuid128_t gopro_control_char_uuid =
    BLE_UUID128_INIT(0xb5, 0xf9, 0x00, 0x72, 0xaa, 0x8d, 0x11, 0xe3,
                     0x90, 0x46, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b);

// Function to send the "Set Shutter" command
static int
gopro_set_shutter(uint16_t conn_handle, uint16_t attr_handle, uint8_t value)
{
    struct os_mbuf *om;
    int rc;

    // Allocate a new mbuf for the write operation
    om = ble_hs_mbuf_from_flat(&value, sizeof(value));
    if (!om) {
        MODLOG_DFLT(ERROR, "Failed to allocate mbuf\n");
        return BLE_HS_ENOMEM;
    }

    // Perform the GATT write
    rc = ble_gattc_write(conn_handle, attr_handle, om, NULL, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to write characteristic; rc=%d\n", rc);
        return rc;
    }

    MODLOG_DFLT(INFO, "Set Shutter command sent successfully\n");
    return 0;
}

void
blecent_on_disc_complete2(const struct peer *peer, int status, void *arg)
{
    const struct peer_chr *chr;

    // Find the control characteristic
    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(0xFEA6), // GoPro service UUID
                             &gopro_control_char_uuid.u);
    if (!chr) {
        MODLOG_DFLT(ERROR, "Control characteristic not found\n");
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    // Send the "Set Shutter" command with parameter 1
    gopro_set_shutter(peer->conn_handle, chr->chr.val_handle, 1);
}
