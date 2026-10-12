/* pcap.c - monitor-mode capture streamed as libpcap over the USB-bridge UART.
 *
 * SCAFFOLD. Receive-only WiFi promiscuous capture. Each frame is queued to a
 * ring buffer; a writer task drains it to UART2 (TX = GPIO1, the USB-bridge
 * pin that reaches COM10). The byte stream is a standard .pcap: a global header
 * then per-packet records, linktype LINKTYPE_IEEE802_11 (105). A PC runs
 * tools/pcap_receiver.py to save it for Wireshark.
 *
 * For networks you own or are authorized to test.
 */
#include "pcap.h"

#include "esp_wifi.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "netscan.h"

#include <string.h>

#define PCAP_UART   UART_NUM_2
#define PCAP_TX     GPIO_NUM_1      /* USB-bridge TX line -> COM10 */
#define PCAP_BAUD   115200
#define SNAPLEN     256
#define RB_SIZE     16384

static bool                 s_run    = false;
static uint8_t              s_ch     = 1;
static RingbufHandle_t      s_rb     = NULL;
static volatile uint32_t    s_pkts   = 0;
static volatile uint32_t    s_drop   = 0;
static bool                 s_uart_ok = false;

typedef struct __attribute__((packed)) {
    uint32_t ts_sec, ts_usec, incl_len, orig_len;
} pcap_rec_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t ver_major, ver_minor;
    int32_t  thiszone;
    uint32_t sigfigs, snaplen, network;
} pcap_gh_t;

static void uart_setup(void)
{
    if (s_uart_ok) return;
    const uart_config_t cfg = {
        .baud_rate = PCAP_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    if (!uart_is_driver_installed(PCAP_UART)) {
        uart_driver_install(PCAP_UART, 256, RB_SIZE, 0, NULL, 0);
        uart_param_config(PCAP_UART, &cfg);
        uart_set_pin(PCAP_UART, PCAP_TX, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    s_uart_ok = true;
}

static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    (void)type;
    const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
    uint32_t len = p->rx_ctrl.sig_len;
    if (len == 0) return;
    uint32_t cap = (len > SNAPLEN) ? SNAPLEN : len;

    uint8_t rec[sizeof(pcap_rec_t) + SNAPLEN];
    pcap_rec_t *h = (pcap_rec_t *)rec;
    int64_t us = esp_timer_get_time();
    h->ts_sec  = (uint32_t)(us / 1000000);
    h->ts_usec = (uint32_t)(us % 1000000);
    h->incl_len = cap;
    h->orig_len = len;
    memcpy(rec + sizeof(pcap_rec_t), p->payload, cap);

    if (xRingbufferSend(s_rb, rec, sizeof(pcap_rec_t) + cap, 0) != pdTRUE) s_drop++;
    else s_pkts++;
}

static void writer_task(void *arg)
{
    (void)arg;
    while (s_run) {
        size_t sz = 0;
        uint8_t *item = (uint8_t *)xRingbufferReceive(s_rb, &sz, pdMS_TO_TICKS(200));
        if (item) {
            uart_write_bytes(PCAP_UART, (const char *)item, sz);
            vRingbufferReturnItem(s_rb, item);
        }
    }
    vTaskDelete(NULL);
}

void pcap_set_channel(uint8_t ch)
{
    if (ch < 1) ch = 1;
    if (ch > 13) ch = 13;
    s_ch = ch;
    if (s_run) esp_wifi_set_channel(s_ch, WIFI_SECOND_CHAN_NONE);
}
uint8_t pcap_get_channel(void) { return s_ch; }
bool    pcap_is_running(void)  { return s_run; }
uint32_t pcap_packets(void)    { return s_pkts; }
uint32_t pcap_dropped(void)    { return s_drop; }

void pcap_start(void)
{
    if (s_run) return;
    netscan_init();        /* bring up WiFi STA */
    uart_setup();

    s_pkts = 0;
    s_drop = 0;
    s_rb = xRingbufferCreate(RB_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (!s_rb) return;

    s_run = true;
    xTaskCreate(writer_task, "pcap_wr", 4096, NULL, 5, NULL);

    esp_wifi_set_promiscuous_rx_cb(&promisc_cb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(s_ch, WIFI_SECOND_CHAN_NONE);

    /* queue the global header first so it leads the stream */
    pcap_gh_t gh = {
        .magic = 0xa1b2c3d4, .ver_major = 2, .ver_minor = 4,
        .thiszone = 0, .sigfigs = 0, .snaplen = SNAPLEN, .network = 105,
    };
    xRingbufferSend(s_rb, &gh, sizeof gh, 0);
}

void pcap_stop(void)
{
    if (!s_run) return;
    esp_wifi_set_promiscuous(false);
    s_run = false;                      /* writer task exits */
    vTaskDelay(pdMS_TO_TICKS(250));
    if (s_rb) { vRingbufferDelete(s_rb); s_rb = NULL; }
}
