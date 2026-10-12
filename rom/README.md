# POCKET RECON — Stage 1 (Game Boy ROM)

A retro WiFi-scanner UI that builds into a real Game Boy ROM (`pocketrecon.gb`)
and runs on the ModRetro Chromatic (or any GB/GBC/emulator). This is **Stage 1**:
the screen, controls, sorting and detail view are all real, but the network data
comes from a **mock source** (`scan_mock.c`) so there's nothing to wire up yet.

> For scanning / inspecting networks and devices **you own or are authorized to
> test.**

## What it does

- Boot splash → press **START** to scan
- Animated "SCANNING…" state, then a scrollable list of access points:
  `> SSID......... ###. W` (cursor · SSID · signal bars · `W`=secured / `o`=open)
- **D-pad ↑/↓** move · **A** open detail (BSSID, channel, RSSI, security) ·
  **B** back · **START** rescan · **SELECT** toggle sort (RSSI ↔ channel)

## Build

You need [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020/releases) (the
Game Boy C toolchain). Unzip it anywhere.

**Windows:** edit `build.bat` so `set GBDK=` points at your GBDK-2020 folder, then:

```bash
build.bat
```

**Linux/macOS:**

```bash
make GBDK_HOME=/path/to/gbdk/
```

Either way you get `pocketrecon.gb`.

## Run

1. **Emulator first** — open `pocketrecon.gb` in Emulicious, BGB, or SameBoy to
   iterate fast.
2. **Real Chromatic** — copy the ROM onto a GB/GBC flashcart (EZ-Flash Jr,
   EverDrive-GB, or a homebrew flash PCB) and boot it in the Chromatic. The ROM
   is a plain 32 KB, no-MBC cart with a GBC-compatible header, so it runs on
   original hardware too.

> Tip: the Chromatic lets you pick a palette in its menu — choose a green one for
> the full Game Boy look.

## How Stage 2 plugs in (no UI rewrite)

Everything the UI knows about networks goes through [`scan.h`](scan.h):

```c
void        scan_begin(void);
uint8_t     scan_is_busy(void);
void        scan_poll(void);
uint8_t     scan_count(void);
const ap_t *scan_get(uint8_t i);
```

- **Now:** `scan_mock.c` implements these from a fake table.
- **Stage 2:** write `scan_cart.c` that implements the *same five functions* by
  reading the ESP32 mailbox over the cartridge bus (the `0xA000–0xBFFF` RAM
  window). Then change one line in the Makefile / build.bat:
  `scan_mock.c` → `scan_cart.c`. `main.c` is untouched.

## Files

| File | Role |
|------|------|
| `main.c` | UI: screens, input, sorting, detail view |
| `scan.h` | the UI ↔ data-source contract (the seam) |
| `scan_mock.c` | Stage 1 fake network source |
| `Makefile` / `build.bat` | build to `pocketrecon.gb` |
