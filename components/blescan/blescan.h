/* blescan.h - passive BLE observer for the WiFi Recon tab's BLE mode.
 *
 * Receive-only: NimBLE in observer role (scan only). It never advertises,
 * never connects, never transmits beyond the controller's passive-scan
 * requirements. Lists nearby advertising devices so you can SEE what's around
 * (e.g. near your phone) and then act on the phone itself.
 *
 * For monitoring devices you own or are authorized to assess.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define BLE_NAME_MAXLEN 18
#define BLE_MAX_DEVS    24

typedef struct {
    uint8_t addr[6];
    int8_t  rssi;
    uint8_t has_name;
    char    name[BLE_NAME_MAXLEN + 1];
} ble_dev_t;

void              blescan_start(void);   /* bring up NimBLE + start passive scan */
void              blescan_stop(void);    /* cancel scan + shut NimBLE down       */
bool              blescan_is_up(void);
uint8_t           blescan_count(void);      /* unique devices seen */
const ble_dev_t  *blescan_get(uint8_t i);
uint32_t          blescan_adv_total(void);  /* total advertisement PDUs seen */
