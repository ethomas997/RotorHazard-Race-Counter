# RotorHazard Race Counter

A battery-friendly ePaper sign that shows the current **heat** and **race number** at the flight line, so
pilots and spectators can always tell which race is up. Designed and built by RocketSled.

The display is a Seeed XIAO ESP32-C3 driving a 5.83" 648×480 monochrome ePaper panel in a 3D-printed
housing with two push-buttons (DN / UP). It can be run standalone from the buttons, or over WiFi from a
small built-in web page and HTTP API. The longer-term goal of this project is to have it follow a
[RotorHazard](https://github.com/RotorHazard/RotorHazard) timer automatically.

## Repository layout

| Path | Contents |
|---|---|
| `firmware/RaceDisplayV2/` | Arduino sketch as received from RocketSled (Aug 9 2026 build, "V2.0"); only the two include names changed (see Known issues) |
| `firmware/RaceDisplayV2/Data/bg.png` | Web-page background image; must be uploaded to the SPIFFS partition separately |
| `firmware/RaceDisplayV2/driver.h` | Seeed_GFX hardware selection (board + panel). **Required** – see Building |
| `firmware/libraries/Seeed_GFX/` | Git submodule: the display library, pinned to a known-good commit |
| `docs/parts-list.md` | Bill of materials |
| `assets/` | Source font (Orbitron Bold, OFL) and logo images used to generate the `.h` bitmaps |

## Hardware

* Seeed Studio **XIAO ESP32-C3**
* Seeed **ePaper Driver Board for XIAO** (V2) – 24-pin FPC, JST battery input with charge IC
* **5.83" monochrome ePaper**, 648×480, UC8179 controller, 24-pin FPC
* Two momentary push-buttons to GND: **DN → D5**, **UP → D6** (internal pull-ups)
* 3D-printed housing (STLs not yet in this repo)

Full list with links: [docs/parts-list.md](docs/parts-list.md).

## Building the firmware

RocketSled builds with Visual Studio + VisualMicro; the sketch also builds in the Arduino IDE 2.x. Settings
below are taken from the VisualMicro project file that shipped with the code, and were verified on
2026-09-20 with `arduino-cli` 0.35.2, esp32 core 3.3.8 and Seeed_GFX 2.0.3 (commit `0dfdd71`):

```
Sketch uses 1256535 bytes (95%) of program storage space. Maximum is 1310720 bytes.
Global variables use 37952 bytes (11%) of dynamic memory.
```

Note the flash usage: the default partition's 1.2 MB app slot is **95 % full**. Any feature work (a
Socket.IO client, more fonts) will need the *Huge APP (3MB No OTA/1MB SPIFFS)* partition scheme instead –
the device doesn't use OTA, so nothing is lost.

Command-line equivalent of the IDE settings below:

```bash
arduino-cli compile -b esp32:esp32:XIAO_ESP32C3:PartitionScheme=default,CDCOnBoot=default --libraries firmware/libraries firmware/RaceDisplayV2
```

1. **Board package:** *esp32 by Espressif Systems* (RocketSled used 3.3.8; board-manager URL
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`). Board: **XIAO_ESP32C3**.
   * Partition Scheme: **Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)**
   * USB CDC On Boot: **Enabled**
2. **Display library:** **[Seeed_GFX](https://github.com/Seeed-Studio/Seeed_GFX)** is not in the Arduino
   Library Manager, so it is vendored here as a git submodule at `firmware/libraries/Seeed_GFX`, pinned
   to a commit known to build. Get it with
   ```bash
   git submodule update --init
   ```
   (or clone with `--recurse-submodules`). Then make it visible to the Arduino IDE by linking or copying it
   into your sketchbook's `libraries` folder – on Windows a directory junction avoids a second copy:
   ```
   mklink /J "<sketchbook>\libraries\Seeed_GFX" "<repo>\firmware\libraries\Seeed_GFX"
   ```
   `arduino-cli` users can skip the link and pass `--libraries firmware/libraries` instead.

   Seeed_GFX is a fork of TFT_eSPI and **conflicts with TFT_eSPI / Adafruit_GFX – remove those from your
   `libraries` folder first.** The sketch will still compile against the wrong library, it just won't drive
   the panel.
3. **`driver.h`** must be in the sketch folder (it is). Seeed_GFX picks it up automatically:
   ```c
   #define BOARD_SCREEN_COMBO 503   // 5.83 inch monochrome ePaper (UC8179), 648x480
   #define USE_XIAO_EPAPER_DRIVER_BOARD
   ```
   If you change the panel or carrier board, regenerate this with Seeed's configuration tool.
4. Compile and upload the sketch over USB-C.
5. **Upload the SPIFFS image.** The web page background lives in the flash filesystem, not the sketch.
   Arduino IDE 2.x has no built-in uploader. Options:
   * a community SPIFFS uploader extension for IDE 2.2.1+, e.g.
     [arduino-spiffs-upload](https://github.com/ivanlee1007/arduino-spiffs-upload) (drop the VSIX into
     `C:\Users\<you>\.arduinoIDE\plugins\`, restart the IDE, then command palette →
     *Upload SPIFFS to Pico/ESP8266/ESP32*). These tools read the sketch's `data/` folder; RocketSled's folder
     is spelled `Data`, which only matters on a case-sensitive filesystem.
   * or build the image with `mkspiffs` (bundled with the esp32 core) and flash it with `esptool` at the
     SPIFFS partition offset of the *Default 4MB* scheme (`0x290000`).
   * VisualMicro has this built in.

   If this step is skipped the device still works – the web page just has a white background.

All other libraries (`WiFi`, `WebServer`, `Preferences`, `SPIFFS`) ship with the esp32 core.

## Operation

### First power-up / WiFi setup

* With no saved credentials the device starts an open access point **`RaceCounter-Setup`** and shows the AP
  IP (`192.168.4.1`) and MAC on the panel. Connect to it, open `http://192.168.4.1`, enter your SSID and
  password, *Save & Reboot*.
* It then connects to that network, shows the IP it was given and its MAC for a few seconds, and displays
  the RotorHazard splash screen. The web interface is at `http://<ip>/`.
* The MAC shown on both the AP screen and the "Connected" screen is the station MAC – the one your router
  sees – so it can be used directly for a static DHCP lease.

### Buttons

| Action | Result |
|---|---|
| UP | race number +1 (1…99, wraps) |
| DN | race number −1 (1…99, wraps) |
| UP + DN together (running) | toggle **PRACTICE** mode |
| UP + DN held at power-on | erase saved WiFi credentials and standalone flag, restart into AP mode |
| UP + DN while "Connecting to SSID…" | same – use this if it is stuck on a network it can't reach |
| UP + DN while in AP mode | enable **standalone mode** (no WiFi, buttons only) and restart |

Heat number can only be set from the web interface / HTTP API.

### Web interface

The main page has −/+ and *Set* controls for Heat and Race, a *Practice* toggle, and *Reset*. The 🛠 icon
top-right opens the settings page where the SSID/password can be changed (*Update*) or erased (*Clear*);
both reboot the device.

### HTTP API

Every endpoint is a plain `GET`, changes the display, and replies `303 → /` (so a browser lands back on the
main page). Scripts should send `allow_redirects=False` or just ignore the redirect.

| Endpoint | Effect |
|---|---|
| `/heatInc`, `/heatDec` | heat ±1 (0…99, wraps); race resets to 1 |
| `/heatSet?v1=N` | set heat to N (0–99). Heat 0 hides the heat number; banner shows "RACE #". Nonzero heat also resets race to 1 |
| `/raceInc`, `/raceDec` | race ±1 (1…99, wraps) |
| `/raceSet?v2=N` | set race to N (1–99) |
| `/practice` | **toggle** practice mode (banner "PRACTICE", big "P") |
| `/splash` | heat 0, race 0, practice off, show the RotorHazard splash screen (this is the web *Reset* button) |
| `/settings` | WiFi settings page |
| `/reset` | **erases the saved WiFi credentials** and reboots into AP mode (the settings-page *Clear* button) |
| `/bg.png` | the SPIFFS background image |

Out-of-range values are ignored. There is no endpoint to *read* the current state.

## Known issues / roadmap

The sketch in this repo is V2.0 as received, with one change: `#include <webserver.h>` /
`<preferences.h>` were corrected to `WebServer.h` / `Preferences.h`. The Arduino build system matches
include names to libraries case-sensitively even on Windows, so the original did not compile in the
Arduino IDE / arduino-cli (VisualMicro is more forgiving). Items identified for a first cleanup pass:

* `/reset` wipes WiFi credentials on a bare `GET` – easy to hit by accident; should be a POST with confirmation.
* The WiFi connect loop never times out (the only escape is holding both buttons).
* `bg.png` (22 KB) could be embedded as a `PROGMEM` array like the logo, eliminating the SPIFFS upload step.
* API is browser-oriented: numeric heat only (RotorHazard heats have names), practice is toggle-only, no
  status endpoint. Needed before RotorHazard can drive it.
* Planned: connect to a RotorHazard server directly (Socket.IO `race_status` / `current_heat` events carry
  the heat id and `next_round`; `/api/heat/<id>` gives the name) so the display follows the timer with no
  plugin required.

## Credits

Hardware, housing and firmware by RocketSled. RotorHazard logo © the RotorHazard project.
Orbitron font by Matt McInerney, SIL Open Font License.
