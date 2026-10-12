/* bt_tools.h - "BT" OSD tab for the Chromatic (BLE scan + sniff). */
#pragma once

#include "osd_shared.h"
#include "button.h"

OSD_Result_t BtTools_Draw(void *arg);
OSD_Result_t BtTools_OnButton(const Button_t Button, const ButtonState_t State, void *arg);
OSD_Result_t BtTools_OnTransition(void *arg);
