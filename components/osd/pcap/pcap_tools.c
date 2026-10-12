/* pcap_tools.c - "PCAP" OSD tab (Wireshark capture) for the Chromatic.
 *
 * A dedicated tab. A = start/stop the capture stream, Up/Down = channel (1-13).
 * While running it streams a .pcap out the USB-bridge UART; a PC runs
 * tools/pcap_receiver.py to save it for Wireshark. Leaving the tab stops the
 * capture and frees the WiFi radio.
 *
 * For networks you own or are authorized to test.
 */
#include "pcap_tools.h"

#include "lvgl.h"
#include "tab_shared.h"
#include "osd_shared.h"
#include "pcap.h"
#include "netscan.h"

#include <stdio.h>

#define ROWS    7
#define ROW_Y0  52
#define ROW_DY  9
#define LEFT_X  8

#define ACCENT lv_color_make(0xFF, 0x99, 0x33)

LV_FONT_DECLARE(chibit_mr);

typedef struct {
    lv_obj_t *pBg, *pFrame, *pHdrBar, *pHdr, *pStatus, *pRows[ROWS];
    lv_timer_t *pTimer;
    bool      entered;
} pui_t;
static pui_t C;

static void set_row(uint8_t i, const char *t)
{
    if (i < ROWS && C.pRows[i]) lv_label_set_text(C.pRows[i], t);
}

static void refresh(void)
{
    char buf[40];

    if (!pcap_is_running()) {
        snprintf(buf, sizeof buf, "CH %u", (unsigned)pcap_get_channel());
        if (C.pStatus) lv_label_set_text(C.pStatus, buf);
        set_row(0, "A: start capture");
        set_row(1, "UP/DN: channel");
        set_row(2, "");
        set_row(3, "streams .pcap");
        set_row(4, "out USB (COM)");
        set_row(5, "run receiver.py");
        set_row(6, "");
        return;
    }

    snprintf(buf, sizeof buf, "CAPTURE ch %u", (unsigned)pcap_get_channel());
    if (C.pStatus) lv_label_set_text(C.pStatus, buf);
    snprintf(buf, sizeof buf, "pkts %u", (unsigned)pcap_packets()); set_row(0, buf);
    snprintf(buf, sizeof buf, "drop %u", (unsigned)pcap_dropped()); set_row(1, buf);
    set_row(2, "");
    set_row(3, "-> COM @115200");
    set_row(4, "open in wireshark");
    set_row(5, "");
    set_row(6, "A: stop");
}

static void timer_cb(lv_timer_t *t) { (void)t; refresh(); }

OSD_Result_t PcapTools_Draw(void *arg)
{
    if (!arg) return kOSD_Result_Err_NullDataPtr;
    lv_obj_t *scr = (lv_obj_t *)((Tab_DrawCtx_t *)arg)->pScreen;

    if (!C.entered) { C.entered = true; }

    if (!C.pBg) {
        C.pBg = lv_obj_create(scr);
        lv_obj_remove_style_all(C.pBg);
        lv_obj_set_size(C.pBg, 160, 120);
        lv_obj_set_pos(C.pBg, 0, 0);
        lv_obj_set_style_bg_color(C.pBg, lv_color_make(0x16, 0x10, 0x06), 0);
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
        lv_obj_set_style_text_color(C.pHdr, lv_color_make(0x20, 0x10, 0x00), 0);
        lv_obj_set_pos(C.pHdr, 8, 27);
        lv_label_set_text_static(C.pHdr, "PCAP");
    }
    if (!C.pStatus) {
        C.pStatus = lv_label_create(scr);
        lv_obj_add_style(C.pStatus, OSD_GetStyleTextWhite(), 0);
        lv_obj_set_style_text_font(C.pStatus, &chibit_mr, 0);
        lv_obj_set_style_text_color(C.pStatus, ACCENT, 0);
        lv_obj_set_pos(C.pStatus, 8, 40);
        lv_label_set_text_static(C.pStatus, "CH 1");
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

OSD_Result_t PcapTools_OnButton(const Button_t b, const ButtonState_t st, void *arg)
{
    (void)arg;
    if (st != kButtonState_Pressed) return kOSD_Result_Ok;
    switch (b) {
        case kButton_Up:
            pcap_set_channel(pcap_get_channel() + 1);
            break;
        case kButton_Down:
            pcap_set_channel(pcap_get_channel() > 1 ? pcap_get_channel() - 1 : 1);
            break;
        case kButton_A:
            if (pcap_is_running()) pcap_stop();
            else                   pcap_start();
            break;
        default:
            break;
    }
    refresh();
    return kOSD_Result_Ok;
}

OSD_Result_t PcapTools_OnTransition(void *arg)
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
    pcap_stop();
    netscan_deinit();
    C.entered = false;
    return kOSD_Result_Ok;
}
