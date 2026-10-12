/* pcap_tools.h - "PCAP" OSD tab (Wireshark capture) for the Chromatic. */
#pragma once

#include "osd_shared.h"
#include "button.h"

OSD_Result_t PcapTools_Draw(void *arg);
OSD_Result_t PcapTools_OnButton(const Button_t Button, const ButtonState_t State, void *arg);
OSD_Result_t PcapTools_OnTransition(void *arg);
