#include "ble_gopro.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static const char *TAG = "BLE_GOPRO_GAP";

/*
 * If not already defined elsewhere, define a helper to convert a BLE address to a string.
 */
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

/**
 * Called when service discovery for a peer has completed.
 */
static void ble_on_disc_complete(const struct peer *peer, int status, void *arg)
{
    MODLOG_DFLT(INFO, "ble_on_disc_complete");
    MODLOG_DFLT(INFO, "connection_handle: %d\n", connected_camera.connection_handle);
    if (status != 0) {
        MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d conn_handle=%d\n",
                    status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    MODLOG_DFLT(INFO, "Service discovery complete; status=%d conn_handle=%d\n",
                status, peer->conn_handle);
                
    MODLOG_DFLT(INFO, "ble_on_disc_compelte end connection_handle: %d\n", connected_camera.connection_handle);
    subscribe_to_characteristics(peer);
    assign_command_handle(peer);
}

/**
 * Connects to the advertiser if it appears to be a GoPro.
 */
void ble_connect(void *disc)
{
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr;

    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

#if CONFIG_EXAMPLE_EXTENDED_ADV
    addr = &((struct ble_gap_ext_disc_desc *)disc)->addr; // Do we need to use this for the GoPro??
#else
    addr = &((struct ble_gap_disc_desc *)disc)->addr;
#endif

    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL,
                         blecent_gap_event, NULL);
    if (rc != 0) {
        char addr_str_buf[18];
        MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d addr=%s; rc=%d\n",
                    addr->type, ble_addr_to_str(addr, addr_str_buf), rc);
        return;
    }
    ESP_LOGI(TAG, "Connection complete!");

}

/**
 * GAP event handler.
 */
int blecent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;
    char addr_str_buf[18];

    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            struct ble_gap_disc_desc *disc = &event->disc;
            ESP_LOGI(TAG, "Discovered device: %s", ble_addr_to_str(&disc->addr, addr_str_buf));

            bool is_gopro = false;
            const uint8_t *adv_data = disc->data;
            uint8_t adv_length = disc->length_data;

            int pos = 0;
            while (pos < adv_length) {
                uint8_t field_len = adv_data[pos];
                if (field_len == 0)
                    break;
                uint8_t field_type = adv_data[pos + 1];

                // Check for 16-bit Service UUIDs (0x02 = Incomplete, 0x03 = Complete)
                if (field_type == 0x02 || field_type == 0x03) {
                    for (int i = 2; i < field_len + 1; i += 2) {
                        uint16_t svc_uuid = adv_data[pos + i] | (adv_data[pos + i + 1] << 8);
                        if (svc_uuid == 0xFEA6) {
                            is_gopro = true;
                        }
                    }
                }
                pos += field_len + 1;
            }

            if (is_gopro) {
                ESP_LOGI(TAG, "GoPro Discovered: %s (RSSI: %d dBm)",
                         ble_addr_to_str(&disc->addr, addr_str_buf), disc->rssi);
                ESP_LOGI(TAG, "Raw Advertisement Data (%d bytes):", adv_length);
                for (int i = 0; i < adv_length; i++) {
                    printf("%02X ", adv_data[i]);
                }
                printf("\n");

                // Process local names if available
                char complete_local_name[31] = {0};
                char shortened_local_name[31] = {0};

                pos = 0;
                while (pos < adv_length) {
                    uint8_t field_len = adv_data[pos];
                    if (field_len == 0)
                        break;
                    uint8_t field_type = adv_data[pos + 1];
                    ESP_LOGI(TAG, "Field Type: 0x%02X, Length: %d", field_type, field_len);
                    if (field_type == 0x09) { // Complete Local Name
                        int name_len = field_len - 1;
                        if (name_len > 30) name_len = 30;
                        memcpy(complete_local_name, &adv_data[pos + 2], name_len);
                        complete_local_name[name_len] = '\0';
                        ESP_LOGI(TAG, "Complete Local Name: %s", complete_local_name);
                    }
                    if (field_type == 0x08) { // Shortened Local Name
                        int name_len = field_len - 1;
                        if (name_len > 30) name_len = 30;
                        memcpy(shortened_local_name, &adv_data[pos + 2], name_len);
                        shortened_local_name[name_len] = '\0';
                        ESP_LOGI(TAG, "Shortened Local Name: %s", shortened_local_name);
                    }
                    if (field_type == 0x02 || field_type == 0x03 || field_type == 0x16) {
                        ESP_LOGI(TAG, "Service UUID Data:");
                        for (int i = 2; i < field_len + 1; i += 2) {
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

        case BLE_GAP_EVENT_EXT_DISC: {
            ESP_LOGI(TAG, "GAP: BLE_GAP_EVENT_EXT_DISC");
            // Process extended discovery if needed.
            return 0;
        }

        // Adjust the event type macro as necessary.
        case BLE_GAP_EVENT_PERIODIC_REPORT: {
            ESP_LOGI(TAG, "GAP: BLE_GAP_EVENT_PERIODIC_ADV_REPORT");
            return 0;
        }

        case BLE_GAP_EVENT_LINK_ESTAB: {
            ESP_LOGI(TAG, "GAP: BLE_GAP_EVENT_LINK_ESTAB");
            if (event->connect.status == 0) {
                MODLOG_DFLT(INFO, "Connection established ");
                // Assign the connection handle to your global camera structure.
                connected_camera.connection_handle = event->connect.conn_handle;
                MODLOG_DFLT(INFO, "Assigned camera connection_handle: %d\n", connected_camera.connection_handle);
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
                print_conn_desc(&desc);
                MODLOG_DFLT(INFO, "\n");
                rc = peer_add(event->connect.conn_handle);
                if (rc != 0) {
                    MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                    return 0;
                }
                rc = ble_gap_security_initiate(event->connect.conn_handle);
                if (rc != 0) {
                    MODLOG_DFLT(INFO, "Security could not be initiated, rc = %d\n", rc);
                    return ble_gap_terminate(event->connect.conn_handle,
                                             BLE_ERR_REM_USER_CONN_TERM);
                } else {
                    MODLOG_DFLT(INFO, "Connection secured\n");
                    MODLOG_DFLT(INFO, "connection_handle: %d\n", connected_camera.connection_handle);
                }
            } else {
                MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n", event->connect.status);
            }
            MODLOG_DFLT(INFO, "connection_handle: %d\n", connected_camera.connection_handle);
            return 0;
        }

        case BLE_GAP_EVENT_DISCONNECT: {
            MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
            print_conn_desc(&event->disconnect.conn);
            MODLOG_DFLT(INFO, "\n");
            peer_delete(event->disconnect.conn.conn_handle);
            return 0;
        }

        case BLE_GAP_EVENT_DISC_COMPLETE: {
            MODLOG_DFLT(INFO, "discovery complete; reason=%d\n", event->disc_complete.reason);
            // Assign the connection handle to your global camera structure.
            connected_camera.connection_handle = event->connect.conn_handle;
            MODLOG_DFLT(INFO, "connection_handle: %d\n", connected_camera.connection_handle);
            return 0;
        }

        case BLE_GAP_EVENT_ENC_CHANGE: {
            MODLOG_DFLT(INFO, "encryption change event; status=%d ", event->enc_change.status);
            MODLOG_DFLT(INFO, "connection_handle: %d\n", connected_camera.connection_handle);
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "connection_handle: %d\n", connected_camera.connection_handle);
            rc = peer_disc_all(event->connect.conn_handle, ble_on_disc_complete, NULL);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
                return 0;
            }
            connected_camera.connection_handle = event->connect.conn_handle;
            MODLOG_DFLT(INFO, "connection_handle: %d\n", connected_camera.connection_handle);
            return 0;
        }

        case BLE_GAP_EVENT_NOTIFY_RX: {
            MODLOG_DFLT(INFO, "received %s; conn_handle=%d attr_handle=%d attr_len=%d\n",
                        event->notify_rx.indication ? "indication" : "notification",
                        event->notify_rx.conn_handle,
                        event->notify_rx.attr_handle,
                        OS_MBUF_PKTLEN(event->notify_rx.om));
            return 0;
        }

        case BLE_GAP_EVENT_MTU: {
            MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                        event->mtu.conn_handle,
                        event->mtu.channel_id,
                        event->mtu.value);
            return 0;
        }

        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            ESP_LOGI(TAG, "GAP: BLE_GAP_EVENT_REPEAT_PAIRING");
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(rc == 0);
            ble_store_util_delete_peer(&desc.peer_id_addr);
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }

        default:
            ESP_LOGI(TAG, "GAP: Unhandled GAP event received! Event Type: %d", event->type);
            return 0;
    }
}
