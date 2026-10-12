/* scan.h - the ONE seam between the UI and "where networks come from".
 *
 * Stage 1 (now):   scan_mock.c   implements this with a fake table.
 * Stage 2 (later): scan_cart.c   implements this by reading the ESP32
 *                  mailbox over the cartridge bus (0xA000-0xBFFF window).
 *
 * The UI in main.c only ever calls these functions, so moving from mock
 * data to a real radio means writing a new backend .c file and changing
 * one line in the Makefile. No UI changes.
 */
#ifndef SCAN_H
#define SCAN_H

#include <stdint.h>

#define SSID_MAXLEN 13   /* 13 chars + NUL = 14 bytes */

typedef struct {
    char    ssid[SSID_MAXLEN + 1];
    uint8_t mac[6];       /* BSSID */
    uint8_t channel;      /* 1..14 */
    int8_t  rssi;         /* dBm, e.g. -42 (closer to 0 = stronger) */
    uint8_t secured;      /* 0 = open, 1 = WPA/WPA2 */
    uint8_t hidden;       /* 1 = SSID not broadcast */
} ap_t;

/* Kick off a new scan. Results become valid once scan_is_busy()==0. */
void          scan_begin(void);

/* 1 while a scan is in progress, 0 when results are ready. */
uint8_t       scan_is_busy(void);

/* Advance the scan state machine. Call once per frame. */
void          scan_poll(void);

/* Number of access points currently in the result set. */
uint8_t       scan_count(void);

/* Pointer to result i (0 <= i < scan_count()). */
const ap_t   *scan_get(uint8_t i);

#endif /* SCAN_H */
