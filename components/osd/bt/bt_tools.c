/* bt_tools.c - "BT" OSD tab for the Chromatic.
 *
 * A dedicated menu tab (sibling of the WiFi tab). Sub-modes: SCAN (list of
 * nearby BLE devices) and SNIFF (live advertisement-PDU counters). Receive-only
 * passive BLE observer (NimBLE). Nothing runs until A; B switches sub-mode and
 * tears down; leaving the tab tears down. Only the BLE stack is ever live here,
 * and switching to the WiFi tab frees it via the tab transition.
 *
 * For monitoring devices you own or are authorized to assess.
 */
#include "bt_tools.h"

#include "lvgl.h"
#include "tab_shared.h"
#include "osd_shared.h"
#include "blescan.h"

#include <stdio.h>

#define ROWS    7
#define ROW_Y0  52
#define ROW_DY  9
#define LEFT_X  8

enum { B_SCAN, B_SNIFF, B_NMODES };

LV_FONT_DECLARE(lv_font_montserrat_26);
LV_FONT_DECLARE(chibit_mr);

#define ACCENT lv_color_make(0x33, 0xCC, 0xFF)

typedef struct {
    lv_obj_t   *pBg, *pFrame, *pHdrBar, *pHdr, *pStatus, *pRows[ROWS];
    lv_timer_t *pTimer;
    uint8_t     sel, top, mode;
    bool        entered, active;
} bui_t;
static bui_t C;

static void set_row(uint8_t i, const char *t)
{
    if (i < ROWS && C.pRows[i]) lv_label_set_text(C.pRows[i], t);
}

static void refresh(void)
{
    char buf[48];

    if (!C.active) {
        if (C.pStatus) lv_label_set_text(C.pStatus, (C.mode == B_SNIFF) ? "SNIFF" : "SCAN");
        set_row(0, "A: start");
        set_row(1, "B: scan / sniff");
        for (uint8_t i = 2; i < ROWS; i++) set_row(i, "");
        return;
    }

    if (C.mode == B_SNIFF) {
        if (C.pStatus) lv_label_set_text(C.pStatus, "SNIFF");
        snprintf(buf, sizeof buf, "adv  %u", (unsigned)blescan_adv_total()); set_row(0, buf);
        snprintf(buf, sizeof buf, "devs %u", (unsigned)blescan_count());     set_row(1, buf);
        for (uint8_t i = 2; i < ROWS; i++) set_row(i, "");
        set_row(6, "B: switch");
        return;
    }

    /* SCAN: device list */
    uint8_t n = blescan_count();
    if (C.pStatus) { snprintf(buf, sizeof buf, "%u devs", (unsigned)n); lv_label_set_text(C.pStatus, buf); }

    if (C.sel >= n && n > 0) C.sel = n - 1;
    if (C.sel < C.top) C.top = C.sel;
    if (C.sel >= C.top + ROWS) C.top = C.sel - (ROWS - 1);

    for (uint8_t i = 0; i < ROWS; i++) {
        if (!C.pRows[i]) continue;
        uint8_t idx = C.top + i;
        if (idx < n) {
            const ble_dev_t *d = blescan_get(idx);
            if (d) {
                const char *nm = d->has_name ? d->name : "(no name)";
                snprintf(buf, sizeof buf, "%c%02X%02X %d %s",
                         (idx == C.sel) ? '>' : ' ', d->addr[4], d->addr[5], (int)d->rssi, nm);
                lv_label_set_text(C.pRows[i], buf);
            }
        } else {
            lv_label_set_text(C.pRows[i], "");
        }
    }
}

static void timer_cb(lv_timer_t *t) { (void)t; refresh(); }

static void stop_active(void)
{
    if (!C.active) return;
    blescan_stop();
    C.active = false;
}

OSD_Result_t BtTools_Draw(void *arg)
{
    if (!arg) return kOSD_Result_Err_NullDataPtr;
    lv_obj_t *scr = (lv_obj_t *)((Tab_DrawCtx_t *)arg)->pScreen;

    if (!C.entered) { C.entered = true; C.active = false; C.mode = B_SCAN; C.sel = 0; C.top = 0; }

    if (!C.pBg) {
        C.pBg = lv_obj_create(scr);
        lv_obj_remove_style_all(C.pBg);
        lv_obj_set_size(C.pBg, 160, 120);
        lv_obj_set_pos(C.pBg, 0, 0);
        lv_obj_set_style_bg_color(C.pBg, lv_color_make(0x08, 0x0E, 0x16), 0);
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
        lv_obj_set_style_text_color(C.pHdr, lv_color_make(0x05, 0x0E, 0x18), 0);
        lv_obj_set_pos(C.pHdr, 8, 27);
        lv_label_set_text_static(C.pHdr, "BT");
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
    if (!C.pTimer) C.pTimer = lv_timer_create(timer_cb, 500, NULL);

    refresh();
    return kOSD_Result_Ok;
}

OSD_Result_t BtTools_OnButton(const Button_t b, const ButtonState_t st, void *arg)
{
    (void)arg;
    if (st != kButtonState_Pressed) return kOSD_Result_Ok;

    bool is_list = C.active && (C.mode == B_SCAN);
    uint8_t n = blescan_count();
    switch (b) {
        case kButton_Down:
            if (is_list && n && C.sel + 1 < n) C.sel++;
            break;
        case kButton_Up:
            if (is_list && C.sel > 0) C.sel--;
            break;
        case kButton_A:
            if (!C.active) {
                blescan_start();       /* scan + sniff share the passive observer */
                C.active = true;
                C.sel = 0; C.top = 0;
            }
            break;
        case kButton_B:
            stop_active();
            C.mode = (uint8_t)((C.mode + 1) % B_NMODES);
            C.sel = 0; C.top = 0;
            break;
        default:
            break;
    }
    refresh();
    return kOSD_Result_Ok;
}

OSD_Result_t BtTools_OnTransition(void *arg)
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
