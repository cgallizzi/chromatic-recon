/* main.c - POCKET RECON
 *
 * A Game Boy ROM (GBDK-2020) that renders a retro WiFi-scanner UI for the
 * ModRetro Chromatic. Stage 1: all data comes from the mock backend in
 * scan_mock.c via the scan.h interface. The screen, input, sorting and
 * detail view below never touch "where the data comes from", so swapping
 * in a real ESP32 cart backend later requires no changes to this file.
 *
 * For scanning / inspecting networks and devices you own or are
 * authorized to test.
 *
 * Screen is 20x18 tiles. Layout:
 *   row 0  : title bar
 *   row 1  : "===================="
 *   row 2  : status (scanning... / FOUND:n SORT:x)
 *   row 3-12: list (10 visible rows)   > SSID........ ####W
 *   row 13 : "--------------------"
 *   row 14 : "A:INFO  START:RESCAN"
 *   row 15 : "SEL:SORT"
 */
#include <gb/gb.h>
#include <gbdk/font.h>
#include <gbdk/console.h>
#include <stdio.h>
#include <stdint.h>
#include "scan.h"

#define MAX_AP    32
#define LIST_TOP  3
#define LIST_ROWS 10

/* --- UI state ------------------------------------------------------------ */
enum { ST_SPLASH, ST_SCAN, ST_LIST, ST_DETAIL };

static uint8_t state;
static uint8_t sel;          /* highlighted index into order[]            */
static uint8_t top;          /* first visible index into order[]          */
static uint8_t sort_mode;    /* 0 = by RSSI (strongest first), 1 = by CH   */
static uint8_t order[MAX_AP];/* display order: indices into scan results   */
static uint8_t need_redraw;

static uint8_t prev_keys;
static uint8_t anim;         /* scanning animation tick                    */

static const char HEX[] = "0123456789ABCDEF";

/* --- small helpers ------------------------------------------------------- */

/* Copy s into dst, truncated or space-padded to exactly w chars + NUL. */
static void pad_str(char *dst, const char *s, uint8_t w) {
    uint8_t i = 0;
    while (s[i] && i < w) { dst[i] = s[i]; i++; }
    while (i < w)         { dst[i] = ' '; i++; }
    dst[i] = 0;
}

/* Map RSSI (dBm) to a 4-cell signal bar string like "###." */
static void build_bars(char *dst, int8_t rssi) {
    uint8_t b, i;
    if      (rssi >= -50) b = 4;
    else if (rssi >= -62) b = 3;
    else if (rssi >= -74) b = 2;
    else if (rssi >= -86) b = 1;
    else                  b = 0;
    for (i = 0; i < 4; i++) dst[i] = (i < b) ? '#' : '.';
    dst[4] = 0;
}

/* Should AP a sort before AP b, under the current sort mode? */
static uint8_t ranks_before(uint8_t a, uint8_t b) {
    const ap_t *A = scan_get(a);
    const ap_t *B = scan_get(b);
    if (sort_mode == 0) {
        return A->rssi > B->rssi;             /* strongest first */
    }
    if (A->channel != B->channel)
        return A->channel < B->channel;       /* low channel first */
    return A->rssi > B->rssi;                 /* tie-break by RSSI */
}

/* Rebuild order[] (selection sort - n is tiny). */
static void sort_order(void) {
    uint8_t n = scan_count();
    uint8_t i, j, best, tmp;
    for (i = 0; i < n; i++) order[i] = i;
    for (i = 0; i < n; i++) {
        best = i;
        for (j = i + 1; j < n; j++)
            if (ranks_before(order[j], order[best])) best = j;
        tmp = order[i]; order[i] = order[best]; order[best] = tmp;
    }
}

/* --- screens ------------------------------------------------------------- */

static void draw_splash(void) {
    cls();
    gotoxy(4, 3);  printf("POCKET RECON");
    gotoxy(3, 5);  printf("wifi field kit");
    gotoxy(5, 8);  printf("authorized");
    gotoxy(3, 9);  printf("networks only");
    gotoxy(4, 13); printf("PRESS START");
    gotoxy(5, 16); printf("v0.1  hw:gb");
}

static void draw_frame(void) {
    cls();
    gotoxy(0, 0);  printf(" POCKET RECON  2.4G");
    gotoxy(0, 1);  printf("====================");
    gotoxy(0, 13); printf("--------------------");
    gotoxy(0, 14); printf("A:INFO  START:RESCAN");
    gotoxy(0, 15); printf("SEL:SORT");
}

static void draw_status(void) {
    uint8_t i;
    gotoxy(0, 2);
    printf("                    ");   /* clear the row */
    gotoxy(0, 2);
    if (scan_is_busy()) {
        printf("SCANNING");
        for (i = 0; i < (uint8_t)(anim & 3); i++) printf(".");
    } else {
        printf("FOUND:%u  SORT:%s", (unsigned)scan_count(),
               sort_mode == 0 ? "RSSI" : "CH");
    }
}

static void draw_row(uint8_t screen_row, uint8_t have, uint8_t ap_index,
                     uint8_t is_sel) {
    char buf[SSID_MAXLEN + 1];
    char bars[5];
    uint8_t y = (uint8_t)(LIST_TOP + screen_row);
    const ap_t *a;

    gotoxy(0, y);
    printf("                    ");   /* clear row first */
    if (!have) return;

    a = scan_get(ap_index);

    gotoxy(0, y);
    printf("%c", is_sel ? '>' : ' ');

    pad_str(buf, a->hidden ? "<hidden>" : a->ssid, 11);
    gotoxy(1, y);
    printf("%s", buf);

    build_bars(bars, a->rssi);
    gotoxy(13, y);
    printf("%s", bars);

    gotoxy(18, y);
    printf("%c", a->secured ? 'W' : 'o');
}

static void draw_list(void) {
    uint8_t n = scan_count();
    uint8_t i, idx;
    draw_status();
    for (i = 0; i < LIST_ROWS; i++) {
        idx = (uint8_t)(top + i);
        if (idx < n)
            draw_row(i, 1, order[idx], (uint8_t)(idx == sel));
        else
            draw_row(i, 0, 0, 0);
    }
}

static void put_mac(const ap_t *a) {
    char m[18];
    uint8_t i, p = 0;
    for (i = 0; i < 6; i++) {
        m[p++] = HEX[a->mac[i] >> 4];
        m[p++] = HEX[a->mac[i] & 0x0F];
        if (i < 5) m[p++] = ':';
    }
    m[p] = 0;
    printf("%s", m);
}

static void draw_detail(void) {
    const ap_t *a = scan_get(order[sel]);
    cls();
    gotoxy(0, 0);  printf("=== AP DETAIL ======");
    gotoxy(0, 2);  printf("SSID:");
    gotoxy(0, 3);  printf("%s", a->hidden ? "<hidden>" : a->ssid);
    gotoxy(0, 5);  printf("BSSID:");
    gotoxy(0, 6);  put_mac(a);
    gotoxy(0, 8);  printf("CHANNEL : %u", (unsigned)a->channel);
    gotoxy(0, 9);  printf("RSSI    : %d dBm", (int)a->rssi);
    gotoxy(0, 10); printf("SECURITY: %s", a->secured ? "WPA/WPA2" : "OPEN");
    gotoxy(0, 15); printf("B:BACK");
}

/* --- input --------------------------------------------------------------- */

static void handle_list_input(uint8_t pressed) {
    uint8_t n = scan_count();

    if (pressed & J_DOWN) {
        if (sel + 1 < n) {
            sel++;
            if (sel >= (uint8_t)(top + LIST_ROWS)) top++;
            need_redraw = 1;
        }
    }
    if (pressed & J_UP) {
        if (sel > 0) {
            sel--;
            if (sel < top) top--;
            need_redraw = 1;
        }
    }
    if (pressed & J_SELECT) {
        sort_mode ^= 1;
        sort_order();
        sel = 0; top = 0;
        need_redraw = 1;
    }
    if (pressed & J_START) {
        scan_begin();
        state = ST_SCAN;
        draw_frame();
    }
    if (pressed & J_A) {
        state = ST_DETAIL;
        draw_detail();
    }
}

/* --- main ---------------------------------------------------------------- */

void main(void) {
    uint8_t keys, pressed;

    font_init();
    font_set(font_load(font_ibm));
    DISPLAY_ON;
    SHOW_BKG;

    state = ST_SPLASH;
    draw_splash();
    prev_keys = joypad();   /* avoid a held key leaking into the first frame */

    for (;;) {
        wait_vbl_done();

        keys = joypad();
        pressed = (uint8_t)(keys & ~prev_keys);
        prev_keys = keys;

        switch (state) {

        case ST_SPLASH:
            if (pressed & J_START) {
                scan_begin();
                sort_mode = 0;
                state = ST_SCAN;
                draw_frame();
            }
            break;

        case ST_SCAN:
            scan_poll();
            anim++;
            if ((anim & 7) == 0) draw_status();   /* animate ~7fps */
            if (!scan_is_busy()) {
                sort_order();
                sel = 0; top = 0;
                state = ST_LIST;
                need_redraw = 1;
            }
            break;

        case ST_LIST:
            handle_list_input(pressed);
            if (need_redraw) {
                draw_list();
                need_redraw = 0;
            }
            break;

        case ST_DETAIL:
            if (pressed & J_B) {
                state = ST_LIST;
                draw_frame();
                need_redraw = 1;
            }
            break;
        }
    }
}
