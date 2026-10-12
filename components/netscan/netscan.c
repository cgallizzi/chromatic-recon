/* netscan.c - esp_wifi backend for the WiFi Recon tab.
 *
 * SCAFFOLD: compiles against ESP-IDF 5.3 (same as the stock firmware) but has
 * only been desk-checked, not run on hardware yet. The radio is a station
 * doing passive/active AP scans. Monitor-mode sniffing (esp_wifi_set_promiscuous)
 * and BLE (NimBLE) are natural follow-ons behind this same seam.
 *
 * NVS is already initialized at boot by Settings_Initialize() in the stock
 * firmware, which esp_wifi needs for PHY calibration data.
 */
#include "netscan.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "netscan";

static bool              s_wifi_up    = false;  /* event/wifi_init done */
static bool              s_started    = false;  /* esp_wifi_start() active     */
static volatile bool     s_busy       = false;
static esp_event_handler_instance_t s_scan_done_h = NULL;

static portMUX_TYPE      s_lock       = portMUX_INITIALIZER_UNLOCKED;
static netscan_ap_t      s_aps[NETSCAN_MAX_APS];
static uint8_t           s_count      = 0;

static bool              s_sniffing   = false;
static uint8_t           s_sniff_ch   = 1;
static netscan_sniff_t   s_sniff;

static int8_t authmode_secured(wifi_auth_mode_t m)
{
    return (m == WIFI_AUTH_OPEN) ? 0 : 1;
}

static void on_scan_done(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;

    uint16_t num = 0;
    esp_wifi_scan_get_ap_num(&num);

    static wifi_ap_record_t recs[NETSCAN_MAX_APS];
    uint16_t want = (num > NETSCAN_MAX_APS) ? NETSCAN_MAX_APS : num;
    if (esp_wifi_scan_get_ap_records(&want, recs) != ESP_OK) {
        want = 0;
    }

    taskENTER_CRITICAL(&s_lock);
    s_count = (uint8_t)want;
    for (uint8_t i = 0; i < s_count; i++) {
        /* ssid is a uint8_t[33], already NUL-terminated by the driver */
        for (int c = 0; c < NETSCAN_SSID_MAXLEN; c++) s_aps[i].ssid[c] = (char)recs[i].ssid[c];
        s_aps[i].ssid[NETSCAN_SSID_MAXLEN] = 0;
        for (int b = 0; b < 6; b++) s_aps[i].bssid[b] = recs[i].bssid[b];
        s_aps[i].channel = recs[i].primary;
        s_aps[i].rssi    = recs[i].rssi;
        s_aps[i].secured = authmode_secured(recs[i].authmode);
        s_aps[i].hidden  = (s_aps[i].ssid[0] == 0) ? 1 : 0;
    }
    taskEXIT_CRITICAL(&s_lock);

    s_busy = false;
    ESP_LOGI(TAG, "scan done: %u APs", (unsigned)s_count);
}

void netscan_init(void)
{
    if (s_wifi_up) {
        if (!s_started) { esp_wifi_start(); s_started = true; }
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    /* default event loop may already exist in the app; tolerate that */
    esp_err_t e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(e);

    /* scan/sniff only: no netif / IP stack needed */

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &on_scan_done, NULL, &s_scan_done_h));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_wifi_up = true;
    s_started = true;
    ESP_LOGI(TAG, "wifi station up");
}

void netscan_deinit(void)
{
    if (!s_wifi_up) return;
    netscan_sniff_stop();
    esp_wifi_scan_stop();
    esp_wifi_stop();
    if (s_scan_done_h) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, s_scan_done_h);
        s_scan_done_h = NULL;
    }
    esp_wifi_deinit();          /* FULL teardown: frees WiFi heap for BLE */
    s_wifi_up = false;
    s_started = false;
    s_busy = false;
    ESP_LOGI(TAG, "wifi fully deinitialized");
}

/* Promiscuous receive callback: receive-only, increments counters by type. */
static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    (void)buf;
    taskENTER_CRITICAL(&s_lock);
    s_sniff.total++;
    switch (type) {
        case WIFI_PKT_MGMT: s_sniff.mgmt++; break;
        case WIFI_PKT_DATA: s_sniff.data++; break;
        case WIFI_PKT_CTRL: s_sniff.ctrl++; break;
        default: break;
    }
    taskEXIT_CRITICAL(&s_lock);
}

void netscan_sniff_start(void)
{
    if (!s_started || s_sniffing) return;
    esp_wifi_scan_stop();
    taskENTER_CRITICAL(&s_lock);
    s_sniff.total = s_sniff.mgmt = s_sniff.data = s_sniff.ctrl = 0;
    s_sniff.channel = s_sniff_ch;
    taskEXIT_CRITICAL(&s_lock);
    esp_wifi_set_promiscuous_rx_cb(&promisc_cb);
    if (esp_wifi_set_promiscuous(true) == ESP_OK) {
        esp_wifi_set_channel(s_sniff_ch, WIFI_SECOND_CHAN_NONE);
        s_sniffing = true;
        s_busy = false;
    }
}

void netscan_sniff_stop(void)
{
    if (!s_sniffing) return;
    esp_wifi_set_promiscuous(false);
    s_sniffing = false;
}

void netscan_sniff_hop(void)
{
    if (!s_sniffing) return;
    s_sniff_ch = (s_sniff_ch % 13) + 1;
    esp_wifi_set_channel(s_sniff_ch, WIFI_SECOND_CHAN_NONE);
    taskENTER_CRITICAL(&s_lock);
    s_sniff.channel = s_sniff_ch;
    taskEXIT_CRITICAL(&s_lock);
}

void netscan_sniff_get(netscan_sniff_t *out)
{
    if (!out) return;
    taskENTER_CRITICAL(&s_lock);
    *out = s_sniff;
    taskEXIT_CRITICAL(&s_lock);
}

void netscan_begin(void)
{
    if (!s_started || s_busy) return;
    s_busy = true;
    wifi_scan_config_t sc = {
        .ssid = NULL, .bssid = NULL, .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    if (esp_wifi_scan_start(&sc, false) != ESP_OK) {
        s_busy = false;
    }
}

bool netscan_is_busy(void) { return s_busy; }

uint8_t netscan_count(void)
{
    taskENTER_CRITICAL(&s_lock);
    uint8_t n = s_count;
    taskEXIT_CRITICAL(&s_lock);
    return n;
}

const netscan_ap_t *netscan_get(uint8_t i)
{
    /* Caller is the LVGL task; data is only replaced wholesale in the event
     * handler, so a bounds check is sufficient for the scaffold. */
    if (i >= s_count) return NULL;
    return &s_aps[i];
}
