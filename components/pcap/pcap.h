/* pcap.h - WiFi monitor-mode capture streamed as a PCAP over UART0 (COM10).
 *
 * Receive-only. Captures 802.11 frames in promiscuous mode and streams a
 * standard libpcap stream (global header + per-packet records, linktype
 * LINKTYPE_IEEE802_11 = 105) out the USB serial so a PC can save a .pcap for
 * Wireshark. See tools/pcap_receiver.py.
 *
 * For networks you own or are authorized to test.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

void      pcap_start(void);           /* bring up WiFi + promiscuous + stream */
void      pcap_stop(void);
bool      pcap_is_running(void);

void      pcap_set_channel(uint8_t ch);   /* 1..13 */
uint8_t   pcap_get_channel(void);

uint32_t  pcap_packets(void);         /* frames captured + queued */
uint32_t  pcap_dropped(void);         /* frames dropped (ring full) */
