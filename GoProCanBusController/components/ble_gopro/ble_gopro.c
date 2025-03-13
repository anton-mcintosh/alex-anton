#include "ble_gopro.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "esp_bt.h"

/* BLE */
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"

#include "peer.h"
#include "misc.h"

static const char *TAG = "BLE_GOPRO";

#ifndef BLE_ADDR_STR_LEN
#define BLE_ADDR_STR_LEN 18
#endif

#ifndef ble_addr_to_str
static inline char *ble_addr_to_str(const ble_addr_t *addr, char *str)
{
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr->val[0],
            addr->val[1],
            addr->val[2],
            addr->val[3],
            addr->val[4],
            addr->val[5]);
    return str;
}
#endif

// Global storage for discovered devices.
char discoveredDevices[MAX_DEVICES][32];
ble_addr_t discoveredDevicesAddr[MAX_DEVICES];
int numDevices = 0;

/*** The UUID of the service containing the subscribable characteristic ***/
static const ble_uuid_t *remote_svc_uuid =
    BLE_UUID128_DECLARE(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                        0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/*** The UUID of the subscribable chatacteristic ***/
static const ble_uuid_t *remote_chr_uuid =
    BLE_UUID128_DECLARE(0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
                        0x22, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33);

static const char *tag = "NimBLE_BLE_CENT";
static int blecent_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t peer_addr[6];

void ble_store_config_init(void);

/**
 * Application Callback. Called when the custom subscribable chatacteristic
 * in the remote GATT server is read.
 * Expect to get the recently written data.
 **/
static int
blecent_on_custom_read(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       struct ble_gatt_attr *attr,
                       void *arg)
{
    MODLOG_DFLT(INFO,
                "Read complete for the subscribable characteristic; "
                "status=%d conn_handle=%d",
                error->status, conn_handle);
    if (error->status == 0)
    {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    MODLOG_DFLT(INFO, "\n");

    return 0;
}

/**
 * Application Callback. Called when the custom subscribable characteristic
 * in the remote GATT server is written to.
 * Client has previously subscribed to this characeteristic,
 * so expect a notification from the server.
 **/
static int
blecent_on_custom_write(uint16_t conn_handle,
                        const struct ble_gatt_error *error,
                        struct ble_gatt_attr *attr,
                        void *arg)
{
    const struct peer_chr *chr;
    const struct peer *peer;
    int rc;

    MODLOG_DFLT(INFO,
                "Write to the custom subscribable characteristic complete; "
                "status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);

    peer = peer_find(conn_handle);
    chr = peer_chr_find_uuid(peer,
                             remote_svc_uuid,
                             remote_chr_uuid);
    if (chr == NULL)
    {
        MODLOG_DFLT(ERROR,
                    "Error: Peer doesn't have the custom subscribable characteristic\n");
        goto err;
    }

    /*** Performs a read on the characteristic, the result is handled in blecent_on_new_read callback ***/
    rc = ble_gattc_read(conn_handle, chr->chr.val_handle,
                        blecent_on_custom_read, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR,
                    "Error: Failed to read the custom subscribable characteristic; "
                    "rc=%d\n",
                    rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application Callback. Called when the custom subscribable characteristic
 * is subscribed to.
 **/
static int
blecent_on_custom_subscribe(uint16_t conn_handle,
                            const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr,
                            void *arg)
{
    const struct peer_chr *chr;
    uint8_t value;
    int rc;
    const struct peer *peer;

    MODLOG_DFLT(INFO,
                "Subscribe to the custom subscribable characteristic complete; "
                "status=%d conn_handle=%d",
                error->status, conn_handle);

    if (error->status == 0)
    {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    MODLOG_DFLT(INFO, "\n");

    peer = peer_find(conn_handle);
    chr = peer_chr_find_uuid(peer,
                             remote_svc_uuid,
                             remote_chr_uuid);
    if (chr == NULL)
    {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't have the subscribable characteristic\n");
        goto err;
    }

    /* Write 1 byte to the new characteristic to test if it notifies after subscribing */
    value = 0x19;
    rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle,
                              &value, sizeof(value), blecent_on_custom_write, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR,
                    "Error: Failed to write to the subscribable characteristic; "
                    "rc=%d\n",
                    rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Performs 3 operations on the remote GATT server.
 * 1. Subscribes to a characteristic by writing 0x10 to it's CCCD.
 * 2. Writes to the characteristic and expect a notification from remote.
 * 3. Reads the characteristic and expect to get the recently written information.
 **/
static void
blecent_custom_gatt_operations(const struct peer *peer)
{
    const struct peer_dsc *dsc;
    int rc;
    uint8_t value[2];

    dsc = peer_dsc_find_uuid(peer,
                             remote_svc_uuid,
                             remote_chr_uuid,
                             BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL)
    {
        MODLOG_DFLT(WARN, "Error: Peer lacks a CCCD for the subscribable characteristic\n");
        return;
        // goto err;
    }

    /*** Write 0x00 and 0x01 (The subscription code) to the CCCD ***/
    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                              value, sizeof(value), blecent_on_custom_subscribe, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR,
                    "Error: Failed to subscribe to the subscribable characteristic; "
                    "rc=%d\n",
                    rc);
        goto err;
    }

    return;
err:
    /* Terminate the connection */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application callback.  Called when the attempt to subscribe to notifications
 * for the ANS Unread Alert Status characteristic has completed.
 */
static int
blecent_on_subscribe(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     struct ble_gatt_attr *attr,
                     void *arg)
{
    struct peer *peer;

    MODLOG_DFLT(INFO, "Subscribe complete; status=%d conn_handle=%d "
                      "attr_handle=%d\n",
                error->status, conn_handle, attr->handle);

    peer = peer_find(conn_handle);
    if (peer == NULL)
    {
        MODLOG_DFLT(ERROR, "Error in finding peer, aborting...");
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    /* Subscribe to, write to, and read the custom characteristic*/
    blecent_custom_gatt_operations(peer);

    return 0;
}

/**
 * Application callback.  Called when the write to the ANS Alert Notification
 * Control Point characteristic has completed.
 */
static int
blecent_on_write(uint16_t conn_handle,
                 const struct ble_gatt_error *error,
                 struct ble_gatt_attr *attr,
                 void *arg)
{
    MODLOG_DFLT(INFO,
                "Write complete; status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);

    /* Subscribe to notifications for the Unread Alert Status characteristic.
     * A central enables notifications by writing two bytes (1, 0) to the
     * characteristic's client-characteristic-configuration-descriptor (CCCD).
     */
    const struct peer_dsc *dsc;
    uint8_t value[2];
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    dsc = peer_dsc_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLECENT_SVC_ALERT_UUID),
                             BLE_UUID16_DECLARE(BLECENT_CHR_UNR_ALERT_STAT_UUID),
                             BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL)
    {
        MODLOG_DFLT(ERROR, "Error: Peer lacks a CCCD for the Unread Alert "
                           "Status characteristic\n");
        goto err;
    }

    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(conn_handle, dsc->dsc.handle,
                              value, sizeof value, blecent_on_subscribe, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "Error: Failed to subscribe to characteristic; "
                           "rc=%d\n",
                    rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection. */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application callback.  Called when the read of the ANS Supported New Alert
 * Category characteristic has completed.
 */
static int
blecent_on_read(uint16_t conn_handle,
                const struct ble_gatt_error *error,
                struct ble_gatt_attr *attr,
                void *arg)
{
    MODLOG_DFLT(INFO, "Read complete; status=%d conn_handle=%d", error->status,
                conn_handle);
    if (error->status == 0)
    {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    MODLOG_DFLT(INFO, "\n");

    /* Write two bytes (99, 100) to the alert-notification-control-point
     * characteristic.
     */
    const struct peer_chr *chr;
    uint8_t value[2];
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLECENT_SVC_ALERT_UUID),
                             BLE_UUID16_DECLARE(BLECENT_CHR_ALERT_NOT_CTRL_PT));
    if (chr == NULL)
    {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support the Alert "
                           "Notification Control Point characteristic\n");
        goto err;
    }

    value[0] = 99;
    value[1] = 100;
    rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle,
                              value, sizeof value, blecent_on_write, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "Error: Failed to write characteristic; rc=%d\n",
                    rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection. */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Performs three GATT operations against the specified peer:
 * 1. Reads the ANS Supported New Alert Category characteristic.
 * 2. After read is completed, writes the ANS Alert Notification Control Point characteristic.
 * 3. After write is completed, subscribes to notifications for the ANS Unread Alert Status
 *    characteristic.
 *
 * If the peer does not support a required service, characteristic, or
 * descriptor, then the peer lied when it claimed support for the alert
 * notification service!  When this happens, or if a GATT procedure fails,
 * this function immediately terminates the connection.
 */
static void
blecent_read_write_subscribe(const struct peer *peer)
{
    const struct peer_chr *chr;
    int rc;

    /* Read the supported-new-alert-category characteristic. */
    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLECENT_SVC_ALERT_UUID),
                             BLE_UUID16_DECLARE(BLECENT_CHR_SUP_NEW_ALERT_CAT_UUID));
    if (chr == NULL)
    {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support the Supported New "
                           "Alert Category characteristic\n");
        goto err;
    }

    rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle,
                        blecent_on_read, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; rc=%d\n",
                    rc);
        goto err;
    }

    return;
err:
    /* Terminate the connection. */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void
blecent_on_disc_complete(const struct peer *peer, int status, void *arg)
{

    if (status != 0)
    {
        /* Service discovery failed.  Terminate the connection. */
        MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d "
                           "conn_handle=%d\n",
                    status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    MODLOG_DFLT(INFO, "Service discovery complete; status=%d "
                      "conn_handle=%d\n",
                status, peer->conn_handle);


        // **Log all discovered services**
    struct peer_svc *svc;
    SLIST_FOREACH(svc, &peer->svcs, next)
    {
        char uuid_str[BLE_UUID_STR_LEN];
        ble_uuid_to_str((ble_uuid_t *)&svc->svc.uuid, uuid_str);
        MODLOG_DFLT(INFO, "Discovered Service: %s (Start Handle: %d, End Handle: %d)",
                    uuid_str, svc->svc.start_handle, svc->svc.end_handle);

        // Log all discovered characteristics inside the service
        struct peer_chr *chr;
        SLIST_FOREACH(chr, &svc->chrs, next)
        {
            ble_uuid_to_str((ble_uuid_t *)&chr->chr.uuid, uuid_str);
            MODLOG_DFLT(INFO, "  Characteristic: %s (Handle: %d, Properties: 0x%X)",
                        uuid_str, chr->chr.val_handle, chr->chr.properties);
        }
    }

    /* Now perform three GATT procedures against the peer: read,
     * write, and subscribe to notifications for the ANS service.
     */

    // Remove the incorrect "Supported New Alert Category" check
    // blecent_read_write_subscribe(peer);
    blecent_custom_gatt_operations(peer);
    
}

/**
 * Initiates the GAP general discovery procedure.
 */
void ble_gopro_scan(void)
{
    ESP_LOGI(TAG, "Initializing scanning...");
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }
    disc_params.filter_duplicates = 1; // Fitler duplicates
    disc_params.passive = 1; // Passive scan
    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0x0010;
    disc_params.window = 0x0010;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, 30 * 1000, &disc_params,
                      blecent_gap_event, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }
}

/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.  A device is "interesting" if it advertises connectability and
 * support for the Alert Notification service.
 */
static void
ble_connect(void *disc)
{
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr;

    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0)
    {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */
#if CONFIG_EXAMPLE_EXTENDED_ADV
    addr = &((struct ble_gap_ext_disc_desc *)disc)->addr;
#else
    addr = &((struct ble_gap_disc_desc *)disc)->addr;
#endif

    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL,
                         blecent_gap_event, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d "
                           "addr=%s; rc=%d\n",
                    addr->type, addr_str(addr->val), rc);
        return;
    }
    ESP_LOGI(TAG, "Connection complete!");
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  blecent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
blecent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
    {
        struct ble_gap_disc_desc *disc = &event->disc;

        ESP_LOGI(TAG, "Discovered device: %s", addr_str(&disc->addr));

        bool is_gopro = false;
        uint8_t *adv_data = disc->data;
        uint8_t adv_length = disc->length_data;

        int pos = 0;
        while (pos < adv_length)
        {
            uint8_t field_len = adv_data[pos];
            if (field_len == 0)
                break;

            uint8_t field_type = adv_data[pos + 1];

            // Check for 16-bit Service UUIDs (0x02 = Incomplete, 0x03 = Complete)
            if (field_type == 0x02 || field_type == 0x03)
            {
                for (int i = 2; i < field_len + 1; i += 2)
                {
                    uint16_t svc_uuid = adv_data[pos + i] | (adv_data[pos + i + 1] << 8);
                    if (svc_uuid == 0xFEA6)
                    {
                        is_gopro = true;
                    }
                }
            }

            pos += field_len + 1;
        }

        // **Only log GoPro devices**
        if (is_gopro)
        {
            ESP_LOGI(TAG, "GoPro Discovered: %s (RSSI: %d dBm)", addr_str(&disc->addr), disc->rssi);
            ESP_LOGI(TAG, "Raw Advertisement Data (%d bytes):", adv_length);

            for (int i = 0; i < adv_length; i++)
            {
                printf("%02X ", adv_data[i]);
            }
            printf("\n");

            char complete_local_name[31] = {0};
            char shortened_local_name[31] = {0};

            pos = 0;
            while (pos < adv_length)
            {
                uint8_t field_len = adv_data[pos];
                if (field_len == 0)
                    break;

                uint8_t field_type = adv_data[pos + 1];

                ESP_LOGI(TAG, "Field Type: 0x%02X, Length: %d", field_type, field_len);

                // Check for Complete Local Name (0x09) or Shortened Local Name (0x08)
                if (field_type == 0x09) // Complete Local Name
                {
                    int name_len = field_len - 1;
                    if (name_len > 30)
                        name_len = 30;
                    memcpy(complete_local_name, &adv_data[pos + 2], name_len);
                    complete_local_name[name_len] = '\0';
                    ESP_LOGI(TAG, "Complete Local Name: %s", complete_local_name);
                }
                if (field_type == 0x08) // Shortened Local Name
                {
                    int name_len = field_len - 1;
                    if (name_len > 30)
                        name_len = 30;
                    memcpy(shortened_local_name, &adv_data[pos + 2], name_len);
                    shortened_local_name[name_len] = '\0';
                    ESP_LOGI(TAG, "Shortened Local Name: %s", shortened_local_name);
                }

                // Check for Service UUIDs (0x03 = 16-bit Complete List, 0x02 = 16-bit Incomplete List, 0x16 = Service Data)
                if (field_type == 0x02 || field_type == 0x03 || field_type == 0x16)
                {
                    ESP_LOGI(TAG, "Service UUID Data:");
                    for (int i = 2; i < field_len + 1; i += 2)
                    {
                        uint16_t svc_uuid = adv_data[pos + i] | (adv_data[pos + i + 1] << 8);
                        ESP_LOGI(TAG, "  UUID: 0x%04X", svc_uuid);
                    }
                }

                pos += field_len + 1;
            }

            ble_connect(disc);
        }
        return 0;
    }

    case BLE_GAP_EVENT_LINK_ESTAB:
        /* A new connection was established or a connection attempt failed. */
        ESP_LOGI(tag, "GAP: BLE_GAP_EVENT_LINK_ESTAB");
        if (event->connect.status == 0)
        {
            /* Connection successfully established. */
            MODLOG_DFLT(INFO, "Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");

            /* Remember peer. */
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0)
            {
                MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }

// #if CONFIG_EXAMPLE_ENCRYPTION
            /** Initiate security - It will perform
             * Pairing (Exchange keys)
             * Bonding (Store keys)
             * Encryption (Enable encryption)
             * Will invoke event BLE_GAP_EVENT_ENC_CHANGE
             **/
            rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc != 0)
            {
                MODLOG_DFLT(INFO, "Security could not be initiated, rc = %d\n", rc);
                return ble_gap_terminate(event->connect.conn_handle,
                                         BLE_ERR_REM_USER_CONN_TERM);
            }
            else
            {
                MODLOG_DFLT(INFO, "Connection secured\n");
            }
// #else
//             /* Perform service discovery */
//             rc = peer_disc_all(event->connect.conn_handle,
//                                blecent_on_disc_complete, NULL);
//             if (rc != 0)
//             {
//                 MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
//                 return 0;
//             }
// #endif
        }
        else
        {
            /* Connection attempt failed; resume scanning. */
            MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n",
                        event->connect.status);
            ble_gopro_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        //ble_gopro_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        MODLOG_DFLT(INFO, "discovery complete; reason=%d\n",
                    event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);

// #if CONFIG_EXAMPLE_ENCRYPTION
        /*** Go for service discovery after encryption has been successfully enabled ***/
        rc = peer_disc_all(event->connect.conn_handle,
                           blecent_on_disc_complete, NULL);
        if (rc != 0)
        {
            MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
            return 0;
        }
// #endif
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Peer sent us a notification or indication. */
        MODLOG_DFLT(INFO, "received %s; conn_handle=%d attr_handle=%d "
                          "attr_len=%d\n",
                    event->notify_rx.indication ? "indication" : "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));

        /* Attribute data is contained in event->notify_rx.om. Use
         * `os_mbuf_copydata` to copy the data received in notification mbuf */
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
    ESP_LOGI(tag, "GAP: BLE_GAP_EVENT_REPEAT_PAIRING");
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

#if CONFIG_EXAMPLE_EXTENDED_ADV
    case BLE_GAP_EVENT_EXT_DISC:
        /* An advertisement report was received during GAP discovery. */
        ext_print_adv_report(&event->disc);

        blec_connect(&event->disc);
        return 0;
#endif


    default:
    ESP_LOGI(tag, "GAP: Unhandled GAP event received! Event Type: %d", event->type);
        return 0;
    }
}

    static void
    blecent_on_reset(int reason)
    {
        MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
    }

    static void
    blecent_on_sync(void)
    {
        int rc;

        /* Make sure we have proper identity address set (public preferred) */
        rc = ble_hs_util_ensure_addr(0);
        assert(rc == 0);
    }

    void ble_host_task(void *param)
    {
        ESP_LOGI(tag, "BLE Host Task Started");
        /* This function will return only when nimble_port_stop() is executed */
        nimble_port_run();

        nimble_port_freertos_deinit();
    }

    // Initialize NimBLE stack
    void ble_gopro_init(void)
    {

        ESP_LOGI(TAG, "Initializing BLE...");

        esp_err_t ret = nimble_port_init();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
            return;
        }

        /* Configure the host. */
        ble_hs_cfg.reset_cb = blecent_on_reset;
        ble_hs_cfg.sync_cb = blecent_on_sync;
        ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

        ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
        ble_hs_cfg.sm_bonding = 1;
        ble_hs_cfg.sm_mitm = 0;
        ble_hs_cfg.sm_sc = 1;

        // /* Initialize data structures to track connected peers. */
        // int rc;
        int rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
        assert(rc == 0);

        // /* Set the default device name. */
        rc = ble_svc_gap_device_name_set("nimble-blegopro");
        assert(rc == 0);

        /* XXX Need to have template for store */
        ble_store_config_init();

        nimble_port_freertos_init(ble_host_task);
    }