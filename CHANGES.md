# Changes from upstream

This is a fork of **ModRetro's `oss-chromatic-console-mcu`** (GPL‑3.0). Per the
GPL, here is a summary of what this fork modifies and adds. The FPGA firmware is
**not** modified.

## New components (added)

- `components/netscan/` — Wi‑Fi station scan (`esp_wifi`) + monitor‑mode sniffer
  (promiscuous frame counters by type, channel hopping). Receive‑only.
- `components/blescan/` — passive BLE observer via NimBLE (device list + RSSI +
  name, advertisement‑PDU counter). Receive‑only; observer role; the BT
  controller is fully torn down on stop so Wi‑Fi can reclaim the RAM.
- `components/pcap/` — monitor‑mode capture streamed as libpcap
  (`LINKTYPE_IEEE802_11`) out UART2 on the USB‑bridge TX pin.
- `components/osd/wifi/`, `components/osd/bt/`, `components/osd/pcap/` — the three
  new menu tabs (LVGL UI), using an explicit press‑A‑to‑start model.

## Modified upstream files

- `components/menu_mgr/menu_mgr.{c,h}` — added `kTabID_WiFi`, `kTabID_BT`,
  `kTabID_PCAP`; added a **code‑drawn dynamic tab strip** (`MenuMgr_DrawStrip`)
  that renders a glyph for every tab, since the stock strip is baked per‑tab art.
- `components/osd/osd_default.c` — registers the three new tabs
  (`CreateMenuWiFi/BT/PCAP`).
- `components/osd/CMakeLists.txt` — builds the new tab sources; adds `netscan`,
  `blescan`, `pcap`, `fonts` requirements.
- `main/main.c` — the 46 KB LVGL/DMA framebuffer is now **heap‑allocated**
  (`heap_caps_malloc`, DMA‑capable) instead of a static array, so it leaves the
  static `.bss` region that otherwise overflows once BT/BLE is enabled.
- `sdkconfig.defaults` — custom 3 MB partition table; enable NimBLE (BLE‑only
  controller); RAM reductions for Wi‑Fi+BLE coexistence (Wi‑Fi code out of IRAM,
  smaller buffers); enable `montserrat_14` for the tab strip glyphs.
- `partitions.csv` (new) — 3 MB `factory` app partition (stock is 1 MB, too small
  with the Bluetooth stack). `nvs`/`phy_init` offsets kept at stock values.

## Not included

- The device's stock firmware backup (`*.bin`) and any captured traffic
  (`*.pcap`) are git‑ignored and are **not** published.
