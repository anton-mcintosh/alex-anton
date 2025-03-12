#ifndef PEER_H
#define PEER_H

#define PEER_ADDR_VAL_SIZE                                  6

struct peer_dsc {
    SLIST_ENTRY(peer_dsc) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(peer_dsc_list, peer_dsc);

struct peer_chr {
    SLIST_ENTRY(peer_chr) next;
    struct ble_gatt_chr chr;

    struct peer_dsc_list dscs;
};
SLIST_HEAD(peer_chr_list, peer_chr);

struct peer_svc {
    SLIST_ENTRY(peer_svc) next;
    struct ble_gatt_svc svc;

    struct peer_chr_list chrs;
};
SLIST_HEAD(peer_svc_list, peer_svc);

struct peer;
typedef void peer_disc_fn(const struct peer *peer, int status, void *arg);

/**
 * @brief The callback function for the devices traversal.
 *
 * @param peer
 * @param arg
 * @return int  0, continue; Others, stop the traversal.
 *
 */
typedef int peer_traverse_fn(const struct peer *peer, void *arg);

struct peer {
    SLIST_ENTRY(peer) next;
    uint16_t conn_handle;

    uint8_t peer_addr[PEER_ADDR_VAL_SIZE];

    /** List of discovered GATT services. */
    struct peer_svc_list svcs;

    /** Keeps track of where we are in the service discovery process. */
    uint16_t disc_prev_chr_val;
    struct peer_svc *cur_svc;

    /** Callback that gets executed when service discovery completes. */
    peer_disc_fn *disc_cb;
    void *disc_cb_arg;
};

void peer_traverse_all(peer_traverse_fn *trav_cb, void *arg);

int peer_disc_svc_by_uuid(uint16_t conn_handle, const ble_uuid_t *uuid, peer_disc_fn *disc_cb,
                          void *disc_cb_arg);

int peer_disc_all(uint16_t conn_handle, peer_disc_fn *disc_cb,
                  void *disc_cb_arg);
const struct peer_dsc *
peer_dsc_find_uuid(const struct peer *peer, const ble_uuid_t *svc_uuid,
                   const ble_uuid_t *chr_uuid, const ble_uuid_t *dsc_uuid);
const struct peer_chr *
peer_chr_find_uuid(const struct peer *peer, const ble_uuid_t *svc_uuid,
                   const ble_uuid_t *chr_uuid);
const struct peer_svc *
peer_svc_find_uuid(const struct peer *peer, const ble_uuid_t *uuid);
int peer_delete(uint16_t conn_handle);
int peer_add(uint16_t conn_handle);
int peer_init(int max_peers, int max_svcs, int max_chrs, int max_dscs);
struct peer *
peer_find(uint16_t conn_handle);

#endif // PEER_H
