/* wifi_tools.h - "WiFi Recon" OSD tab for the Chromatic.
 *
 * Registered in osd_default.c exactly like the stock Status/Display/System
 * tabs. Draw/Button/Transition match the OSD widget callback contract
 * (see osd_shared.h). The tab owns the radio lifecycle via netscan.
 */
#pragma once

#include "osd_shared.h"
#include "button.h"

/* arg is a Tab_DrawCtx_t* (see tab_shared.h) - we use its pScreen. */
OSD_Result_t WifiTools_Draw(void *arg);

/* arg is the tab's TabCollection_t* (unused). Routed buttons: A/B/Up/Down. */
OSD_Result_t WifiTools_OnButton(const Button_t Button, const ButtonState_t State, void *arg);

/* Tear down LVGL objects + stop the radio when leaving the tab. */
OSD_Result_t WifiTools_OnTransition(void *arg);
