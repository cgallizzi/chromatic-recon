/* blescan.c - passive BLE observer (NimBLE) for the BLE mode.
 *
 * SCAFFOLD. Receive-only: NimBLE host in observer role doing a passive scan.
 * It collects advertising reports (address, RSSI, name) and dedups by address.
 * It never advertises, scans actively, or connects.
 *
 * Controller is BLE-only (CONFIG_BTDM_CTRL_MODE_BLE_ONLY) to save RAM. BLE mode
 * is entered only after WiFi is stopped (the tab calls netscan_deinit first),
 * so the two radios are not contending.
 */
#include "blescan.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_bt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/util/util.h"

#include <string.h>

static const char   *TAG = "blescan";
static bool          s_up = false;
static bool          s_classic_released = false;   /* CLASSIC mem can be freed only once */
static uint8_t       s_own_addr_type;

static portMUX_TYPE  s_lock = portMUX_INITIALIZER_UNLOCKED;
static ble_dev_t     s_devs[BLE_MAX_DEVS];
static uint8_t       s_count = 0;
static uint32_t      s_adv_total = 0;

static int  gap_event(struct ble_gap_event *event, void *arg);

static void add_report(const uint8_t *addr, int8_t rssi,
                       const uint8_t *name, uint8_t name_len)
{
    taskENTER_CRITICAL(&s_lock);
    s_adv_total++;
    int idx = -1;
    for (uint8_t i = 0; i < s_count; i++) {
        if (memcmp(s_devs[i].addr, addr, 6) == 0) { idx = i; break; }
    }
    if (idx < 0 && s_count < BLE_MAX_DEVS) {
        idx = s_count++;
        memcpy(s_devs[idx].addr, addr, 6);
        s_devs[idx].has_name = 0;
        s_devs[idx].name[0] = 0;
    }
    if (idx >= 0) {
        s_devs[idx].rssi = rssi;
        if (name && name_len) {
            uint8_t n = (name_len > BLE_NAME_MAXLEN) ? BLE_NAME_MAXLEN : name_len;
            memcpy(s_devs[idx].name, name, n);
            s_devs[idx].name[n] = 0;
            s_devs[idx].has_name = 1;
        }
    }
    taskEXIT_CRITICAL(&s_lock);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, event->disc.data,
                                    event->disc.length_data) == 0) {
            add_report(event->disc.addr.val, event->disc.rssi,
                       fields.name, fields.name_len);
        } else {
            add_report(event->disc.addr.val, event->disc.rssi, NULL, 0);
        }
    }
    return 0;
}

static void start_disc(void)
{
    struct ble_gap_disc_params p = {0};
    p.passive = 1;            /* receive-only, no scan requests */
    p.filter_duplicates = 0;
    p.itvl = 0;
    p.window = 0;
    ble_gap_disc(s_own_addr_type, BLE_HS_FOREVER, &p, gap_event, NULL);
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    if (ble_hs_id_infer_auto(0, &s_own_addr_type) == 0) {
        start_disc();
    }
}

static void on_reset(int reason) { (void)reason; }

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();                 /* blocks until nimble_port_stop() */
    nimble_port_freertos_deinit();
}

void blescan_start(void)
{
    if (s_up) return;

    taskENTER_CRITICAL(&s_lock);
    s_count = 0;
    s_adv_total = 0;
    taskEXIT_CRITICAL(&s_lock);

    /* reclaim the unused Classic-BT controller memory for the BLE heap -
     * only valid ONCE; a second release double-frees it and corrupts the heap */
    if (!s_classic_released) {
        esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        s_classic_released = true;
    }

    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed");
        return;
    }
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    nimble_port_freertos_init(host_task);
    s_up = true;
}

void blescan_stop(void)
{
    if (!s_up) return;
    ble_gap_disc_cancel();
    if (nimble_port_stop() == 0) {
        nimble_port_deinit();
    }
    /* nimble_port_deinit leaves the BT controller initialized; fully tear it
     * down (status-guarded so it's safe whether or not it's already done) so
     * the next blescan_start can re-init cleanly instead of aborting. */
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        esp_bt_controller_disable();
    }
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
        esp_bt_controller_deinit();
    }
    s_up = false;
}

bool blescan_is_up(void) { return s_up; }

uint8_t blescan_count(void)
{
    taskENTER_CRITICAL(&s_lock);
    uint8_t n = s_count;
    taskEXIT_CRITICAL(&s_lock);
    return n;
}

const ble_dev_t *blescan_get(uint8_t i)
{
    if (i >= s_count) return NULL;
    return &s_devs[i];
}

uint32_t blescan_adv_total(void)
{
    taskENTER_CRITICAL(&s_lock);
    uint32_t v = s_adv_total;
    taskEXIT_CRITICAL(&s_lock);
    return v;
}
