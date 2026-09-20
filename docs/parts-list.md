# Parts list

## Electronics

| Qty | Part | Notes |
|---|---|---|
| 1 | [Seeed Studio XIAO ESP32-C3](https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html) | Male headers required to plug into the driver board – use the [pre-soldered](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32C3-Pre-Soldered-p-6331.html) variant or solder your own. Includes the 2.4 GHz FPC antenna with U.FL lead, which is used as-is (stuck to the inside wall of the case). |
| 1 | [Seeed ePaper Driver Board for XIAO (V2)](https://www.seeedstudio.com/ePaper-breakout-Board-for-XIAO-V2-p-6374.html) | 24-pin FPC connector, JST 2-pin `BAT` input with `ON/OFF` switch, built-in charge IC. |
| 1 | [5.83" monochrome ePaper display, 648×480](https://www.seeedstudio.com/5-83-Monochrome-ePaper-Display-with-648x480-Pixels-p-5785.html) | UC8179 controller, 24-pin FPC. Any UC8179 648×480 24-pin panel (e.g. GoodDisplay GDEY0583T81) is electrically equivalent. |
| 2 | Panel-mount momentary push-button, black domed cap, blue threaded body, solder-tab terminals | DN and UP, through the top of the case. Wired to GND: DN → D5, UP → D6. |
| 1 | Panel-mount latching (push-on / push-off) push-button, black | Power switch, right side of the case. In series with the battery + lead to the driver board's `BAT` input. |
| 1 | 18650 Li-ion cell, 3.7 V | Fitted cell is labelled "5800 mAh"; realistic capacity for an 18650 is 2000–3000 mAh. ≈ 25 mA draw with WiFi off. Charged through the driver board's charge IC whenever USB is connected. |
| 1 | 18650 single-cell holder with wire leads | Hot-glued to the left inside wall. |
| 1 | JST 2.0 mm 2-pin battery lead | Holder / power switch → driver board `BAT`. |
| 1 | USB-C male plug breakout board | Inserted into the XIAO's USB-C port. |
| 1 | USB-C female socket breakout board | Hot-glued at the bottom-right corner as the external charge / programming port; 4 wires (`GND`, `D-`, `D+`, `VBUS`) to the plug breakout. |
| — | Hook-up wire (24–26 AWG for buttons/power, 30 AWG wire-wrap for USB data), hot glue | |

## Mechanical

| Qty | Part | Notes |
|---|---|---|
| 1 | 3D-printed housing, ≈ 5.4" wide | Yellow body with black hazard stripes (two-colour print); "Race Counter" script and DN / UP labels. STLs to be added. |
| 1 | 3D-printed back plate, red | RotorHazard logo. |
| 1 | Clear front window | Striped bezel printed on / under it. |
| 4 | Black cap screws, front corners | |
| 2 | Rubber bumper feet, bottom edge | |

## Software & tools

* Arduino IDE 2.x, or Visual Studio + [VisualMicro](https://www.visualmicro.com/)
* esp32 Arduino core 3.3.x — board *XIAO_ESP32C3*, partition *Huge APP (3MB No OTA)*, *USB CDC On Boot: Enabled* (all recorded in `firmware/RaceCounter/sketch.yaml`)
* [Seeed_GFX](https://github.com/Seeed-Studio/Seeed_GFX) library (git submodule in `firmware/libraries/`; conflicts with TFT_eSPI / Adafruit_GFX)
* `driver.h` in the sketch folder (in the repo)
* Orbitron Bold TTF (`assets/fonts/`) + Adafruit `fontconvert`, for regenerating the `Orbitron*pt7b.h` headers
* `tools/bin2header.py`, for regenerating the embedded web-page background (`bg_png.h`)
