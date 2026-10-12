#include "osd_default.h"

#include "battery.h"
#include "menu_mgr.h"
#include "tab_shared.h"
#include "tab_list.h"
#include "tab_dots.h"
#include "tab_table.h"
#include "status/fw.h"
#include "status/brightness.h"
#include "controls/dpad_ctl.h"
#include "controls/hotkeys.h"
#include "palette/gbc_color_temp.h"
#include "palette/style.h"
#include "display/color_correct_lcd.h"
#include "display/color_correct_usb.h"
#include "display/frameblend.h"
#include "display/low_batt_icon_ctl.h"
#include "display/screen_transit_ctl.h"
#include "system/silent.h"
#include "system/player_num.h"
#include "system/serial_num.h"
#include "wifi/wifi_tools.h"
#include "bt/bt_tools.h"
#include "pcap/pcap_tools.h"
#include "osd.h"
#include "osd_shared.h"
#include "esp_log.h"

#include <stdint.h>

static const char *TAG = "OSDDef";

LV_IMG_DECLARE(menu_status);
LV_IMG_DECLARE(menu_display);
LV_IMG_DECLARE(menu_controls);
LV_IMG_DECLARE(menu_palette);
LV_IMG_DECLARE(menu_palette_gbc);
LV_IMG_DECLARE(menu_system);
LV_IMG_DECLARE(img_dot_white);

static void CreateMenuStatus(lv_obj_t *const pScreen);
static void CreateMenuDisplay(lv_obj_t *const pScreen);
static void CreateMenuControls(lv_obj_t *const pScreen);
static void CreateMenuPalette(lv_obj_t *const pScreen);
static void CreateMenuColorTemp(lv_obj_t *const pScreen);
static void CreateMenuSystem(lv_obj_t *const pScreen);
static void CreateMenuWiFi(lv_obj_t *const pScreen);
static void CreateMenuBT(lv_obj_t *const pScreen);
static void CreateMenuPCAP(lv_obj_t *const pScreen);
static bool IsPaletteTabActive(void);
static bool IsColorTempTabActive(void);

void OSD_Default_Init(lv_obj_t *const pScreen)
{
    static OSD_Widget_t MenuMgr = {
        .Name = "MenuMgr",
    };

    OSD_Result_t eResult = MenuMgr_Initialize(&MenuMgr, pScreen);

    if (eResult != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "Menu manager init failed: %d", eResult);
        return;
    }

    static OSD_Widget_t Battery = {
        .Name = "Battery",
    };

    if ((eResult = Battery_Init(&Battery)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "Battery widget init failed %d", eResult);
        return;
    }

    CreateMenuStatus(pScreen);
    CreateMenuDisplay(pScreen);
    CreateMenuControls(pScreen);
    CreateMenuPalette(pScreen);
    CreateMenuColorTemp(pScreen);
    CreateMenuSystem(pScreen);
    CreateMenuWiFi(pScreen);
    CreateMenuBT(pScreen);
    CreateMenuPCAP(pScreen);

    OSD_AddWidget(&Battery);
    OSD_AddWidget(&MenuMgr);

    ESP_LOGI(TAG, "Default OSD init OK");
}

static void CreateMenuStatus(lv_obj_t *const pScreen)
{
    static TabCollection_t StatusList;
    TabCollection_t *pList = &StatusList;
    OSD_Result_t eResult;

    sys_dlist_init(&StatusList.WidgetList);

    static MenuTab_t Tab_Status = {
        .Widget = {
            .Name = "MenuStatus",
            .fnDraw = TabList_Draw,
            .fnOnButton = Tab_OnButton,
            .fnOnTransition =  TabList_OnTransition,
        },
        .pImageDesc = &menu_status,
        .Menu = &StatusList
    };
    Tab_Status.Accent = lv_color_make(0xFF, 0, 0);

    if ((eResult = MenuMgr_AddTab(kTabID_Status, &Tab_Status)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_Status.Widget.Name, eResult);
        return;
    }

    static TabItem_t Brightness = {
        .Widget = {
            .Name = "BRIGHTNESS",
            .fnDraw = Brightness_Draw,
            .fnOnTransition = Brightness_OnTransition,
            .fnOnButton = Brightness_OnButton,
        },
    };

    if ((eResult = Tab_AddItem(pList, &Brightness, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", Tab_Status.Widget.Name, eResult);
        return;
    }

    static TabItem_t SilentMode = {
        .Widget = {
            .Name = "SILENT MODE",
            .fnDraw = SilentMode_Draw,
            .fnOnButton = SilentMode_OnButton,
            .fnOnTransition = SilentMode_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &SilentMode, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", SilentMode.Widget.Name, eResult);
        return;
    }

}

static void CreateMenuDisplay(lv_obj_t *const pScreen)
{
    static TabCollection_t DisplayList;
    TabCollection_t *pList = &DisplayList;
    OSD_Result_t eResult;

    sys_dlist_init(&DisplayList.WidgetList);

    static MenuTab_t Tab_Display = {
        .Widget = {
            .Name = "MenuDisplay",
            .fnDraw = TabList_Draw,
            .fnOnButton = Tab_OnButton,
            .fnOnTransition =  TabList_OnTransition,
        },
        .pImageDesc = &menu_display,
        .Menu = &DisplayList,
    };
    Tab_Display.Accent = lv_color_make(0xFF, 0xFF, 0);

    if ((eResult = MenuMgr_AddTab(kTabID_Display, &Tab_Display)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_Display.Widget.Name, eResult);
        return;
    }

    static TabItem_t FrameBlending = {
        .Widget = {
            .Name = "FRAME BLEND",
            .fnDraw = FrameBlend_Draw,
            .fnOnButton = FrameBlend_OnButton,
            .fnOnTransition = FrameBlend_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &FrameBlending, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", FrameBlending.Widget.Name, eResult);
        return;
    }

    #ifdef ENABLE_COLOR_CORRECTION
    static TabItem_t ColorCorrection_LCD = {
        .Widget = {
            .Name = "LCD COLOR CORR.",
            .fnDraw = ColorCorrectLCD_Draw,
            .fnOnButton = ColorCorrectLCD_OnButton,
            .fnOnTransition = ColorCorrectLCD_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &ColorCorrection_LCD, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", ColorCorrection_LCD.Widget.Name, eResult);
        return;
    }
    #endif

    static TabItem_t ColorCorrection_USB = {
        .Widget = {
            .Name = "STREAMING",
            .fnDraw = ColorCorrectUSB_Draw,
            .fnOnButton = ColorCorrectUSB_OnButton,
            .fnOnTransition = ColorCorrectUSB_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &ColorCorrection_USB, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", ColorCorrection_USB.Widget.Name, eResult);
        return;
    }

    static TabItem_t ScreenTransitCtl = {
        .Widget = {
            .Name = "TRANSITIONS",
            .fnDraw = ScreenTransitCtl_Draw,
            .fnOnButton = ScreenTransitCtl_OnButton,
            .fnOnTransition = ScreenTransitCtl_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &ScreenTransitCtl, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", ScreenTransitCtl.Widget.Name, eResult);
        return;
    }

    static TabItem_t LowBattIconCtl = {
        .Widget = {
            .Name = "LOW BATTERY",
            .fnDraw = LowBattIconCtl_Draw,
            .fnOnButton = LowBattIconCtl_OnButton,
            .fnOnTransition = LowBattIconCtl_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &LowBattIconCtl, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", LowBattIconCtl.Widget.Name, eResult);
        return;
    }
}

static void CreateMenuControls(lv_obj_t *const pScreen)
{
    static TabCollection_t ControlsList;
    TabCollection_t *pList = &ControlsList;

    sys_dlist_init(&ControlsList.WidgetList);
    static MenuTab_t Tab_Controls = {
        .Widget = {
            .Name = "MenuControls",
            .fnDraw = TabDot_Draw,
            .fnOnButton = Tab_OnButton,
            .fnOnTransition =  TabDot_OnTransition,
        },
        .Menu = &ControlsList,
        .pImageDesc = &menu_controls,
    };
    Tab_Controls.Accent = lv_color_make(0, 0xCC, 0xFF);

    OSD_Result_t eResult;

    static TabItem_t DPad = {
        .Widget = {
            .Name = "D-PAD",
            .fnDraw = DPadCtl_Draw,
            .fnOnTransition = DPadCtl_OnTransition,
            .fnOnButton = DPadCtl_OnButton,
        },
    };

    if ((eResult = Tab_AddItem(pList, &DPad, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", DPad.Widget.Name, eResult);
        return;
    }

    static TabItem_t HotKeysBrightness = {
        .Widget = {
            .Name = "HK: BRIGHTNESS",
            .fnDraw = HotKeys_DrawBrightness,
            .fnOnTransition = HotKeys_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &HotKeysBrightness, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", HotKeysBrightness.Widget.Name, eResult);
        return;
    }

    static TabItem_t HotKeysColorTemp = {
        .Widget = {
            .Name = "HK: PAL/COLOR TEMP",
            .fnDraw = HotKeys_DrawColorTemp,
            .fnOnTransition = HotKeys_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &HotKeysColorTemp, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", HotKeysColorTemp.Widget.Name, eResult);
        return;
    }

    static TabItem_t HotKeysResetEmu = {
        .Widget = {
            .Name = "HK: RESET EMU",
            .fnDraw = HotKeys_DrawResetEmulation,
            .fnOnTransition = HotKeys_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &HotKeysResetEmu, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", HotKeysResetEmu.Widget.Name, eResult);
        return;
    }

    if ((eResult = MenuMgr_AddTab(kTabID_Controls, &Tab_Controls)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_Controls.Widget.Name, eResult);
        return;
    }

}

static void CreateMenuPalette(lv_obj_t *const pScreen)
{
    static TabCollection_t PaletteListGB;
    static TabItem_t PaletteOpts[kNumPalettes];
    static MenuTab_t Tab_Palette = {
        .Widget = {
            .Name = "MenuPalette",
            .fnDraw = TabTable_Draw,
            .fnOnButton = Tab_OnButton,
            .fnOnTransition =  TabList_OnTransition,  // TabTable has same object lifetime behavior as TabList
        },
        .pImageDesc = &menu_palette,
        .Menu = &PaletteListGB,
        .fnIsActive = IsPaletteTabActive,
    };

    OSD_Result_t eResult;

    sys_dlist_init(&PaletteListGB.WidgetList);

    Tab_Palette.Accent = lv_color_make(0x92, 0x4E, 0xD7);  // Unused in actual selection

    const char *PaletteStyleNames[kNumPalettes] = {
        [kPalette_Default]   = "DEFAULT",
        [kPalette_Brown]     = "BROWN",          // Up
        [kPalette_Blue]      = "BLUE",           // Left
        [kPalette_Pastel]    = "PASTEL",         // Down
        [kPalette_Green]     = "GREEN",          // Right
        [kPalette_Red]       = "RED",            // Up + A
        [kPalette_DarkBlue]  = "DARK BLUE",      // Left + A 
        [kPalette_Orange]    = "ORANGE",         // Down + A
        [kPalette_DarkGreen] = "DARK GREEN",     // Right + A
        [kPalette_DarkBrown] = "DARK BROWN",     // Up + B
        [kPalette_Grayscale] = "GRAYSCALE",      // Left + B
        [kPalette_Yellow]    = "YELLOW",         // Down + B
        [kPalette_Negative]  = "NEGATIVE",       // Right + B
        [kPalette_DMG1]      = "DMG1: GAMEBOY",  // Right + A + B
        [kPalette_DMG2]      = "DMG2: GAMEBOY"   // Left + A + B
    };

    for (StyleID_t ID = 0; ID < kNumPalettes; ID++)
    {
        PaletteOpts[ID] = (TabItem_t){
            .Widget = {
                .Name = PaletteStyleNames[ID],
                .fnDraw = Style_Draw,
                .fnOnButton = Style_OnButton,
                .fnOnTransition = Style_OnTransition,
            },
        };

        if ((eResult = Tab_AddItem(&PaletteListGB, &PaletteOpts[ID], pScreen)) != kOSD_Result_Ok)
        {
            ESP_LOGE(TAG, "%s tab item init failed %d", PaletteOpts[ID].Widget.Name, eResult);
            return;
        }
    }

    if ((eResult = MenuMgr_AddTab(kTabID_Palette, &Tab_Palette)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_Palette.Widget.Name, eResult);
        return;
    }
}

static void CreateMenuColorTemp(lv_obj_t *const pScreen)
{
    static TabCollection_t ColorTempList;
    static TabItem_t ColorTempItem;
    static MenuTab_t Tab_ColorTemp = {
        .Widget = {
            .Name = "MenuColorTemp",
            .fnDraw = TabList_Draw,
            .fnOnButton = Tab_OnButton,
            .fnOnTransition = TabList_OnTransition,
        },
        .pImageDesc = &menu_palette_gbc,
        .Menu = &ColorTempList,
        .fnIsActive = IsColorTempTabActive,
    };

    OSD_Result_t eResult;

    sys_dlist_init(&ColorTempList.WidgetList);

    Tab_ColorTemp.Accent = lv_color_make(0x92, 0x4E, 0xD7);

    ColorTempItem = (TabItem_t){
        .Widget = {
            .Name = "COLOR TEMP",
            .fnDraw = GBCColorTemp_Draw,
            .fnOnButton = GBCColorTemp_OnButton,
            .fnOnTransition = GBCColorTemp_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(&ColorTempList, &ColorTempItem, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", ColorTempItem.Widget.Name, eResult);
        return;
    }

    if ((eResult = MenuMgr_AddTab(kTabID_ColorTemp, &Tab_ColorTemp)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_ColorTemp.Widget.Name, eResult);
        return;
    }
}

static bool IsPaletteTabActive(void)
{
    return !Style_IsGBCMode();
}

static bool IsColorTempTabActive(void)
{
    return Style_IsGBCMode();
}

static void CreateMenuSystem(lv_obj_t *const pScreen)
{
    static TabCollection_t SystemList;
    TabCollection_t *pList = &SystemList;
    OSD_Result_t eResult;

    sys_dlist_init(&SystemList.WidgetList);

    static MenuTab_t Tab_System = {
        .Widget = {
            .Name = "MenuSystem",
            .fnDraw = TabList_Draw,
            .fnOnButton = Tab_OnButton,
            .fnOnTransition = TabList_OnTransition,
        },
        .pImageDesc = &menu_system,
        .Menu = &SystemList,
    };
    Tab_System.Accent = lv_color_make(0xFF, 0x33, 0x99);

    static TabItem_t Firmware = {
        .Widget = {
            .Name = "FIRMWARE",
            .fnDraw = Firmware_Draw,
            .fnOnTransition = Firmware_OnTransition,
            .fnOnButton = Firmware_OnButton,
        },
    };

    if ((eResult = Tab_AddItem(pList, &Firmware, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", Firmware.Widget.Name, eResult);
        return;
    }

    static TabItem_t PlayerNum = {
        .Widget = {
            .Name = "PLAYER #",
            .fnDraw = PlayerNum_Draw,
            .fnOnButton = PlayerNum_OnButton,
            .fnOnTransition = PlayerNum_OnTransition,
        },
    };

    if ((eResult = Tab_AddItem(pList, &PlayerNum, pScreen)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s tab item init failed %d", PlayerNum.Widget.Name, eResult);
        return;
    }

    if (SerialNum_IsPresent())
    {
        static TabItem_t SerialNumber = {
            .Widget = {
                .Name = "SERIAL #",
                .fnDraw = SerialNum_Draw,
                .fnOnTransition = SerialNum_OnTransition,
            },
        };

        if ((eResult = Tab_AddItem(pList, &SerialNumber, pScreen)) != kOSD_Result_Ok)
        {
            ESP_LOGE(TAG, "%s tab item init failed %d", SerialNumber.Widget.Name, eResult);
            return;
        }
    }

    if ((eResult = MenuMgr_AddTab(kTabID_System, &Tab_System)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_System.Widget.Name, eResult);
        return;
    }
}

static void CreateMenuWiFi(lv_obj_t *const pScreen)
{
    (void)pScreen;
    static TabCollection_t WiFiList;   /* unused by the self-drawn WiFi tab */
    sys_dlist_init(&WiFiList.WidgetList);

    static MenuTab_t Tab_WiFi = {
        .Widget = {
            .Name           = "MenuWiFi",
            .fnDraw         = WifiTools_Draw,
            .fnOnButton     = WifiTools_OnButton,
            .fnOnTransition = WifiTools_OnTransition,
        },
        .pImageDesc = &img_dot_white,  /* blank slot; wifi glyph drawn by WifiTools_Draw */
        .Menu       = &WiFiList,
    };
    Tab_WiFi.Accent = lv_color_make(0x00, 0xFF, 0x66);

    OSD_Result_t eResult = MenuMgr_AddTab(kTabID_WiFi, &Tab_WiFi);
    if (eResult != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_WiFi.Widget.Name, eResult);
        return;
    }
}

static void CreateMenuBT(lv_obj_t *const pScreen)
{
    (void)pScreen;
    static TabCollection_t BTList;
    sys_dlist_init(&BTList.WidgetList);

    static MenuTab_t Tab_BT = {
        .Widget = {
            .Name           = "MenuBT",
            .fnDraw         = BtTools_Draw,
            .fnOnButton     = BtTools_OnButton,
            .fnOnTransition = BtTools_OnTransition,
        },
        .pImageDesc = &img_dot_white,  /* blank slot; BT glyph drawn by the tab */
        .Menu       = &BTList,
    };
    Tab_BT.Accent = lv_color_make(0x33, 0xCC, 0xFF);

    OSD_Result_t eResult = MenuMgr_AddTab(kTabID_BT, &Tab_BT);
    if (eResult != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_BT.Widget.Name, eResult);
        return;
    }
}

static void CreateMenuPCAP(lv_obj_t *const pScreen)
{
    (void)pScreen;
    static TabCollection_t PcapList;
    sys_dlist_init(&PcapList.WidgetList);

    static MenuTab_t Tab_Pcap = {
        .Widget = {
            .Name           = "MenuPCAP",
            .fnDraw         = PcapTools_Draw,
            .fnOnButton     = PcapTools_OnButton,
            .fnOnTransition = PcapTools_OnTransition,
        },
        .pImageDesc = &img_dot_white,  /* blank slot; strip draws the glyph */
        .Menu       = &PcapList,
    };
    Tab_Pcap.Accent = lv_color_make(0xFF, 0x99, 0x33);

    OSD_Result_t eResult = MenuMgr_AddTab(kTabID_PCAP, &Tab_Pcap);
    if (eResult != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "%s widget init failed %d", Tab_Pcap.Widget.Name, eResult);
        return;
    }
}
