/* netscan.h - radio seam for the Chromatic WiFi Recon tab.
 *
 * Same shape as the Stage-1 GB ROM's scan.h, but backed by esp_wifi on the
 * console's onboard ESP32-MINI-1. The OSD tab (wifi_tools.c) only ever calls
 * these functions; all radio code lives in netscan.c.
 *
 * The radio is brought up by netscan_init() and torn down by netscan_deinit(),
 * which the WiFi tab calls on enter/exit so the 2.4 GHz radio is only powered
 * while you are actually on that screen (never during gameplay).
 *
 * For scanning networks you own or are authorized to test.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define NETSCAN_SSID_MAXLEN 32
#define NETSCAN_MAX_APS     20

typedef struct {
    char    ssid[NETSCAN_SSID_MAXLEN + 1];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t  rssi;      /* dBm */
    uint8_t secured;   /* 0 = open, 1 = any WPA/WEP */
    uint8_t hidden;    /* 1 = empty SSID */
} netscan_ap_t;

/* Bring up / tear down the WiFi station (idempotent). deinit stops the radio. */
void                 netscan_init(void);
void                 netscan_deinit(void);

/* Kick off an async scan; results valid once netscan_is_busy()==false. */
void                 netscan_begin(void);
bool                 netscan_is_busy(void);

/* Thread-safe accessors (snapshot of the last completed scan). */
uint8_t              netscan_count(void);
const netscan_ap_t  *netscan_get(uint8_t i);

/* --- Passive sniffer (WiFi monitor mode) --------------------------------
 * Counts captured 802.11 frames by type on your own airspace. Receive-only;
 * it never transmits. Call netscan_sniff_hop() periodically to channel-hop.
 */
typedef struct {
    uint32_t total;
    uint32_t mgmt;   /* beacons, probes, assoc, etc. */
    uint32_t data;
    uint32_t ctrl;
    uint8_t  channel;
} netscan_sniff_t;

void                 netscan_sniff_start(void);
void                 netscan_sniff_stop(void);
void                 netscan_sniff_hop(void);          /* advance channel 1..13 */
void                 netscan_sniff_get(netscan_sniff_t *out);
