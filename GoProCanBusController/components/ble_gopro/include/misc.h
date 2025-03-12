#ifndef MISC_H
#define MISC_H

#include <stdint.h>
#include "nimble/ble.h"
#include "host/ble_gap.h"


void print_bytes(const uint8_t *bytes, int len);
void print_mbuf(const struct os_mbuf *om);
void print_mbuf_data(const struct os_mbuf *om);
char *addr_str(const void *addr);
void print_uuid(const ble_uuid_t *uuid);
void print_conn_desc(const struct ble_gap_conn_desc *desc);
void print_adv_fields(const struct ble_hs_adv_fields *fields);
void ext_print_adv_report(const void *param);

#endif // MISC_H