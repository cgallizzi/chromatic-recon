/* wifi_tools.c - "WiFi" OSD tab for the Chromatic.
 *
 * A dedicated menu tab (sibling of the BT tab). Sub-modes: SCAN (AP list) and
 * SNIFF (passive monitor-mode frame counters). Nothing runs until the user
 * presses A; B switches sub-mode and tears down whatever was running; leaving
 * the tab tears everything down. So only the WiFi stack is ever live here, and
 * switching to the BT tab frees it via the tab transition.
 *
 * For networks you own or are authorized to test.
 */
#include "wifi_tools.h"

#include "lvgl.h"
#include "tab_shared.h"
#include "osd_shared.h"
#include "netscan.h"

#include <stdio.h>

#define ROWS    7
#define ROW_Y0  52
#define ROW_DY  9
#define LEFT_X  8

enum { W_SCAN, W_SNIFF, W_NMODES };

LV_FONT_DECLARE(lv_font_montserrat_26);
LV_FONT_DECLARE(chibit_mr);

#define ACCENT lv_color_make(0x00, 0xFF, 0x66)

typedef struct {
    lv_obj_t   *pBg, *pFrame, *pHdrBar, *pHdr, *pStatus, *pRows[ROWS];
    lv_timer_t *pTimer;
    uint8_t     sel, top, anim, mode;
    bool        entered, active;
} wui_t;
static wui_t C;

static void set_row(uint8_t i, const char *t)
{
    if (i < ROWS && C.pRows[i]) lv_label_set_text(C.pRows[i], t);
}

static void bars_str(char *d, int8_t r)
{
    uint8_t b;
    if      (r >= -50) b = 4;
    else if (r >= -62) b = 3;
    else if (r >= -74) b = 2;
    else if (r >= -86) b = 1;
    else               b = 0;
    for (uint8_t i = 0; i < 4; i++) d[i] = (i < b) ? '#' : '.';
    d[4] = 0;
}

static void refresh(void)
{
    char buf[48];

    if (!C.active) {
        if (C.pStatus) lv_label_set_text(C.pStatus, (C.mode == W_SNIFF) ? "SNIFF" : "SCAN");
        set_row(0, "A: start");
        set_row(1, "B: scan / sniff");
        for (uint8_t i = 2; i < ROWS; i++) set_row(i, "");
        return;
    }

    if (C.mode == W_SNIFF) {
        netscan_sniff_t s;
        netscan_sniff_get(&s);
        if (C.pStatus) lv_label_set_text(C.pStatus, "SNIFF");
        snprintf(buf, sizeof buf, "ch %u",   (unsigned)s.channel); set_row(0, buf);
        snprintf(buf, sizeof buf, "pkts %u", (unsigned)s.total);   set_row(1, buf);
        snprintf(buf, sizeof buf, "mgmt %u", (unsigned)s.mgmt);    set_row(2, buf);
        snprintf(buf, sizeof buf, "data %u", (unsigned)s.data);    set_row(3, buf);
        snprintf(buf, sizeof buf, "ctrl %u", (unsigned)s.ctrl);    set_row(4, buf);
        set_row(5, "");
        set_row(6, "B: switch");
        return;
    }

    /* SCAN */
    if (netscan_is_busy()) {
        C.anim++;
        char d[4] = {0};
        for (uint8_t i = 0; i < (uint8_t)(C.anim % 4); i++) d[i] = '.';
        snprintf(buf, sizeof buf, "scanning%s", d);
    } else {
        snprintf(buf, sizeof buf, "%u found", (unsigned)netscan_count());
    }
    if (C.pStatus) lv_label_set_text(C.pStatus, buf);

    uint8_t n = netscan_count();
    if (C.sel >= n && n > 0) C.sel = n - 1;
    if (C.sel < C.top) C.top = C.sel;
    if (C.sel >= C.top + ROWS) C.top = C.sel - (ROWS - 1);

    for (uint8_t i = 0; i < ROWS; i++) {
        if (!C.pRows[i]) continue;
        uint8_t idx = C.top + i;
        if (idx < n) {
            const netscan_ap_t *a = netscan_get(idx);
            if (a) {
                char ss[12];
                const char *s = a->hidden ? "<hidden>" : a->ssid;
                uint8_t k = 0;
                while (s[k] && k < 11) { ss[k] = s[k]; k++; }
                while (k < 11) ss[k++] = ' ';
                ss[11] = 0;
                char br[5];
                bars_str(br, a->rssi);
                snprintf(buf, sizeof buf, "%c%s %s%c",
                         (idx == C.sel) ? '>' : ' ', ss, br, a->secured ? 'W' : 'o');
                lv_label_set_text(C.pRows[i], buf);
            }
        } else {
            lv_label_set_text(C.pRows[i], "");
        }
    }
}

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    if (C.active && C.mode == W_SNIFF) netscan_sniff_hop();
    refresh();
}

static void stop_active(void)
{
    if (!C.active) return;
    netscan_sniff_stop();
    netscan_deinit();
    C.active = false;
}

OSD_Result_t WifiTools_Draw(void *arg)
{
    if (!arg) return kOSD_Result_Err_NullDataPtr;
    lv_obj_t *scr = (lv_obj_t *)((Tab_DrawCtx_t *)arg)->pScreen;

    if (!C.entered) { C.entered = true; C.active = false; C.mode = W_SCAN; C.sel = 0; C.top = 0; }

    if (!C.pBg) {
        C.pBg = lv_obj_create(scr);
        lv_obj_remove_style_all(C.pBg);
        lv_obj_set_size(C.pBg, 160, 120);
        lv_obj_set_pos(C.pBg, 0, 0);
        lv_obj_set_style_bg_color(C.pBg, lv_color_make(0x0A, 0x12, 0x0C), 0);
        lv_obj_set_style_bg_opa(C.pBg, LV_OPA_COVER, 0);
        lv_obj_clear_flag(C.pBg, LV_OBJ_FLAG_SCROLLABLE);
    }
    if (!C.pFrame) {
        C.pFrame = lv_obj_create(scr);
        lv_obj_remove_style_all(C.pFrame);
        lv_obj_set_size(C.pFrame, 154, 94);
        lv_obj_set_pos(C.pFrame, 3, 24);
        lv_obj_set_style_bg_opa(C.pFrame, LV_OPA_0, 0);
        lv_obj_set_style_border_color(C.pFrame, ACCENT, 0);
        lv_obj_set_style_border_width(C.pFrame, 1, 0);
        lv_obj_set_style_radius(C.pFrame, 2, 0);
        lv_obj_clear_flag(C.pFrame, LV_OBJ_FLAG_SCROLLABLE);
    }
    if (!C.pHdrBar) {
        C.pHdrBar = lv_obj_create(scr);
        lv_obj_remove_style_all(C.pHdrBar);
        lv_obj_set_size(C.pHdrBar, 152, 12);
        lv_obj_set_pos(C.pHdrBar, 4, 25);
        lv_obj_set_style_bg_color(C.pHdrBar, ACCENT, 0);
        lv_obj_set_style_bg_opa(C.pHdrBar, LV_OPA_COVER, 0);
        lv_obj_clear_flag(C.pHdrBar, LV_OBJ_FLAG_SCROLLABLE);
    }
    if (!C.pHdr) {
        C.pHdr = lv_label_create(scr);
        lv_obj_remove_style_all(C.pHdr);
        lv_obj_set_style_text_font(C.pHdr, &chibit_mr, 0);
        lv_obj_set_style_text_color(C.pHdr, lv_color_make(0x05, 0x14, 0x0A), 0);
        lv_obj_set_pos(C.pHdr, 8, 27);
        lv_label_set_text_static(C.pHdr, "WIFI");
    }
    if (!C.pStatus) {
        C.pStatus = lv_label_create(scr);
        lv_obj_add_style(C.pStatus, OSD_GetStyleTextWhite(), 0);
        lv_obj_set_style_text_font(C.pStatus, &chibit_mr, 0);
        lv_obj_set_style_text_color(C.pStatus, ACCENT, 0);
        lv_obj_set_pos(C.pStatus, 8, 40);
        lv_label_set_text_static(C.pStatus, "SCAN");
    }
    for (uint8_t i = 0; i < ROWS; i++) {
        if (!C.pRows[i]) {
            C.pRows[i] = lv_label_create(scr);
            lv_obj_add_style(C.pRows[i], OSD_GetStyleTextWhite(), 0);
            lv_obj_set_style_text_font(C.pRows[i], &chibit_mr, 0);
            lv_obj_set_pos(C.pRows[i], LEFT_X, ROW_Y0 + i * ROW_DY);
            lv_label_set_text_static(C.pRows[i], "");
        }
    }
    if (!C.pTimer) C.pTimer = lv_timer_create(timer_cb, 400, NULL);

    refresh();
    return kOSD_Result_Ok;
}

OSD_Result_t WifiTools_OnButton(const Button_t b, const ButtonState_t st, void *arg)
{
    (void)arg;
    if (st != kButtonState_Pressed) return kOSD_Result_Ok;

    bool is_list = C.active && (C.mode == W_SCAN);
    uint8_t n = netscan_count();
    switch (b) {
        case kButton_Down:
            if (is_list && n && C.sel + 1 < n) C.sel++;
            break;
        case kButton_Up:
            if (is_list && C.sel > 0) C.sel--;
            break;
        case kButton_A:
            if (!C.active) {
                netscan_init();
                if (C.mode == W_SNIFF) netscan_sniff_start();
                else                   netscan_begin();
                C.active = true;
                C.sel = 0; C.top = 0;
            } else if (C.mode == W_SCAN) {
                netscan_begin();   /* rescan */
            }
            break;
        case kButton_B:
            stop_active();
            C.mode = (uint8_t)((C.mode + 1) % W_NMODES);
            C.sel = 0; C.top = 0;
            break;
        default:
            break;
    }
    refresh();
    return kOSD_Result_Ok;
}

OSD_Result_t WifiTools_OnTransition(void *arg)
{
    (void)arg;
    if (C.pTimer) { lv_timer_del(C.pTimer); C.pTimer = NULL; }
    lv_obj_t **o[] = { &C.pBg, &C.pFrame, &C.pHdrBar, &C.pHdr, &C.pStatus };
    for (size_t i = 0; i < sizeof o / sizeof o[0]; i++) {
        if (*o[i]) { lv_obj_del(*o[i]); *o[i] = NULL; }
    }
    for (uint8_t i = 0; i < ROWS; i++) {
        if (C.pRows[i]) { lv_obj_del(C.pRows[i]); C.pRows[i] = NULL; }
    }
    stop_active();
    C.entered = false;
    return kOSD_Result_Ok;
}
