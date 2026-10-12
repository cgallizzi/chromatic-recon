/* scan_mock.c - Stage 1 fake scan source.
 *
 * Implements scan.h with a static table of made-up access points and a
 * short artificial "scanning" delay so the UI's scanning animation has
 * something to show. On each completed scan the RSSI values jitter a
 * little so re-scans feel live.
 *
 * Nothing here touches real hardware. When Stage 2 arrives, this file is
 * replaced by scan_cart.c, which fills the same ap_t records from the
 * ESP32 on the cartridge.
 */
#include "scan.h"

#define MOCK_N   14
#define SCAN_FRAMES 75   /* ~1.25s at 60fps of "scanning" */

/* Deliberately generic / obviously-fake SSIDs. */
static ap_t aps[MOCK_N] = {
    { "HOME-5AED",    {0x5A,0xED,0x9C,0x11,0x22,0x33}, 6,  -41, 1, 0 },
    { "PIXEL_NET",    {0x3C,0x28,0x6D,0xAA,0xBB,0xCC}, 1,  -55, 1, 0 },
    { "NETGEAR42",    {0x20,0xE8,0x74,0x01,0x02,0x03}, 11, -63, 1, 0 },
    { "FREE_WIFI",    {0xDC,0xA6,0x32,0x44,0x55,0x66}, 6,  -70, 0, 0 },
    { "linksys",      {0x00,0x14,0xBF,0x10,0x20,0x30}, 3,  -78, 0, 0 },
    { "CoffeeShop",   {0xF0,0x9F,0xC2,0x77,0x88,0x99}, 9,  -66, 1, 0 },
    { "IoT-cam01",    {0xB8,0x27,0xEB,0xAB,0xCD,0xEF}, 1,  -82, 1, 0 },
    { "GuestWPA2",    {0x44,0x07,0x0B,0x12,0x34,0x56}, 11, -59, 1, 0 },
    { "TP-LINK_99",   {0x50,0xC7,0xBF,0x9A,0x8B,0x7C}, 4,  -74, 1, 0 },
    { "DIRECT-GB",    {0x7A,0x11,0x22,0x33,0x44,0x55}, 6,  -48, 1, 0 },
    { "",             {0x02,0xDE,0xAD,0xBE,0xEF,0x01}, 2,  -88, 1, 1 }, /* hidden */
    { "ESP32-TEST",   {0x24,0x6F,0x28,0x0A,0x0B,0x0C}, 8,  -52, 0, 0 },
    { "oldrouter",    {0x00,0x1D,0x7E,0xDE,0xAD,0x11}, 13, -91, 0, 0 },
    { "Hall-AP-2",    {0x9C,0x3D,0xCF,0x65,0x43,0x21}, 1,  -69, 1, 0 },
};

static uint8_t  busy_frames = 0;
static uint16_t rng = 0xACE1;

static uint8_t rnd(void) {
    /* 16-bit xorshift, low byte returned */
    rng ^= (uint16_t)(rng << 7);
    rng ^= (uint16_t)(rng >> 9);
    rng ^= (uint16_t)(rng << 8);
    return (uint8_t)rng;
}

void scan_begin(void) {
    busy_frames = SCAN_FRAMES;
}

uint8_t scan_is_busy(void) {
    return busy_frames != 0;
}

void scan_poll(void) {
    uint8_t i;
    int8_t  j, v;

    if (busy_frames == 0)
        return;

    busy_frames--;
    if (busy_frames != 0)
        return;

    /* Scan just finished: jitter RSSI +/-3 dB so the list feels alive. */
    for (i = 0; i < MOCK_N; i++) {
        j = (int8_t)(rnd() % 7) - 3;
        v = (int8_t)(aps[i].rssi + j);
        if (v > -20) v = -20;
        if (v < -95) v = -95;
        aps[i].rssi = v;
    }
}

uint8_t scan_count(void) {
    return MOCK_N;
}

const ap_t *scan_get(uint8_t i) {
    return &aps[i];
}
