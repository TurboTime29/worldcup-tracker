# ⚽ World Cup 2026 Tracker

A glanceable **FIFA World Cup 2026 score tracker** for the round
**Waveshare ESP32-S3-Touch-LCD-1.28** (240×240 GC9A01 display). It shows one
match per screen — finished results, live scores, and upcoming kickoffs in your
local time — and you swipe through the whole tournament.

No coding required to use it: flash it from your browser, then enter your Wi-Fi
and a free data token on the device itself.

<p align="center">
  <!-- Add a photo of the device here once you have one: -->
  <!-- <img src="docs/device.jpg" width="320" alt="World Cup Tracker on the round display"> -->
</p>

## Features

- **Whole tournament, swipeable** — every match that has happened plus every
  scheduled fixture, sorted by kickoff. Swipe left/right to browse.
- **Auto-jumps to the action** — on boot it lands on a live match, or the next
  upcoming one, or the most recent result.
- **Live / finished / upcoming states** — scores, winner/draw/penalty-shootout,
  flags, and kickoff time in **your local timezone** (auto-detected).
- **12- or 24-hour clock** — chosen during setup.
- **On-device brightness control** — a single click of the BOOT button opens a
  brightness screen; no reflashing to dim it.
- **First-boot Wi-Fi setup** — a captive portal; credentials persist in flash.
- **Nothing personal is baked into the firmware** — your Wi-Fi password and API
  token are entered on the device and stored only in its flash (NVS). They are
  never compiled in and are not present in the released binary.

## Hardware

- **Waveshare ESP32-S3-Touch-LCD-1.28** — round 1.28" 240×240 GC9A01 SPI
  display with a CST816 capacitive touch panel and an onboard ESP32-S3.
- A USB-C cable. That's it.

## Flash it (no tools to install)

The easiest way is the **browser web installer** (Chrome or Edge on
desktop — they support Web Serial):

### 👉 [Open the Web Installer](https://TurboTime29.github.io/worldcup-tracker/)

1. Plug the board into your computer with USB-C.
2. Open the link above, click **Connect**, and pick the serial port.
3. Click **Install** and wait for it to finish.

Prefer the command line? Download `firmware-merged.bin` from the
[latest release](https://github.com/TurboTime29/worldcup-tracker/releases/latest) and
flash it with [esptool](https://github.com/espressif/esptool):

```bash
esptool.py --chip esp32s3 write_flash 0x0 firmware-merged.bin
```

## First-time setup (on the device)

On first boot (or after a Wi-Fi reset) the tracker creates a Wi-Fi hotspot:

1. On your phone or laptop, join the **`WorldCup-Setup`** network.
2. A setup page opens automatically (or browse to `http://192.168.4.1`).
3. Fill in:
   - **football-data.org API token** (free — see below)
   - **Wi-Fi network** and **password**
   - **Clock format** (12- or 24-hour)
4. Tap **Save & Connect**. The device reboots, joins your Wi-Fi, and starts
   showing matches within a few seconds.

To change any setting later while it's connected, browse to
`http://worldcup.local` on the same network.

### Getting a free data token

Match data comes from [football-data.org](https://www.football-data.org/), which
has a free tier (10 requests/minute, no daily cap) that includes the World Cup.

1. Register at <https://www.football-data.org/client/register>.
2. Copy the API token from your account email/dashboard.
3. Paste it into the setup page.

You can verify a token works with:

```bash
curl -s "https://api.football-data.org/v4/competitions/WC/matches?dateFrom=2026-06-11&dateTo=2026-06-18" \
     -H "X-Auth-Token: YOUR_TOKEN"
```

## Controls

| Action | What it does |
|---|---|
| **Swipe left / right** | Previous / next match |
| **Tap the screen** | Re-center on the live/next match |
| **Single click BOOT** | Open / close the brightness screen (−/+ and DONE) |
| **Hold BOOT ~3 s** | Wipe Wi-Fi credentials and reopen the setup portal (your token is kept) |

## Build from source

Built with [PlatformIO](https://platformio.org/).

```bash
# Build
pio run -e worldcup

# Flash over USB (replace COMx / port with yours)
pio run -e worldcup -t upload --upload-port COMx

# Produce the single merged image used by the web installer
pio run -e worldcup -t merge
# -> .pio/build/worldcup/firmware-merged.bin
```

### How it works

- **Data**: one `GET /v4/competitions/WC/matches` per ~7-day window, fetched at
  boot over a single reused TLS connection, parsed with a streaming
  [ArduinoJson](https://arduinojson.org/) filter into a compact match list.
- **Polling**: to respect the free tier, it fetches the full tournament at boot
  and then refreshes it about every 6 hours (~4×/day), so newly-scheduled
  fixtures — like knockout matchups once the teams are known — appear
  automatically. It polls faster while a match is live or just after a kickoff
  time passes.
- **Timezone**: the local UTC offset is detected once via ip-api.com; kickoff
  times and countdowns are shown in local time.
- **Flags**: country flags are embedded as small RGB565 bitmaps generated from
  [flagcdn.com](https://flagcdn.com/) by `scripts/build_flags.py`.
- **Rendering**: [LovyanGFX](https://github.com/lovyan03/LovyanGFX) drives the
  GC9A01 panel; the screen only repaints when its contents actually change, so
  it stays flicker-free between updates.

## Privacy & credentials

This project is designed so that **none of your secrets ever leave your device**:

- Your Wi-Fi password and API token are entered through the on-device setup
  portal and stored only in the ESP32's NVS flash partition.
- They are **not** compiled into the firmware and are **not** included in the
  released `firmware-merged.bin` (which contains only the bootloader, partition
  table, and application).
- The repository and release contain no API keys, Wi-Fi credentials, or other
  personal data.

## Credits

- Match data: [football-data.org](https://www.football-data.org/)
- Flags: [flagcdn.com](https://flagcdn.com/)
- Display driver: [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- JSON parsing: [ArduinoJson](https://arduinojson.org/)
- Browser flashing: [ESP Web Tools](https://esphome.github.io/esp-web-tools/)

## License

[MIT](LICENSE).

This is an independent hobby project and is **not affiliated with, endorsed by,
or sponsored by** FIFA or football-data.org. "World Cup" and related marks
belong to their respective owners.
