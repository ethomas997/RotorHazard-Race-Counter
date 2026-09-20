# Parts list

## Electronics

| Qty | Part | Notes |
|---|---|---|
| 1 | [Seeed Studio XIAO ESP32-C3](https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html) | Male headers required to plug into the driver board – use the [pre-soldered](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32C3-Pre-Soldered-p-6331.html) variant or solder your own. Includes the U.FL 2.4 GHz antenna. |
| 1 | [Seeed ePaper Driver Board for XIAO (V2)](https://www.seeedstudio.com/ePaper-breakout-Board-for-XIAO-V2-p-6374.html) | 24-pin FPC connector, JST 2-pin battery input with power switch, built-in charge IC. |
| 1 | [5.83" monochrome ePaper display, 648×480](https://www.seeedstudio.com/5-83-Monochrome-ePaper-Display-with-648x480-Pixels-p-5785.html) | UC8179 controller, 24-pin FPC. Any UC8179 648×480 24-pin panel (e.g. GoodDisplay GDEY0583T81) is electrically equivalent. |
| 2 | 12 mm momentary push-button, black dome cap | Wired to GND: DN → D5, UP → D6. |
| 1 | USB-C cable | Programming and power. |

## Mechanical

| Qty | Part | Notes |
|---|---|---|
| 1 | 3D-printed housing, black, ≈ 5.4" wide | Hazard-stripe bezel with "Race Counter" script; DN / UP labels on the top edge. STLs to be added. |
| 1 | Clear front window | |
| 4 | Black cap screws, front corners | |
| 2 | Rubber bumper feet, bottom edge | |

## Optional

| Qty | Part | Notes |
|---|---|---|
| 1 | 3.7 V Li-ion / LiPo battery (e.g. 18650, 3000 mAh) + holder | Connects to the driver board's JST battery input. ≈ 25 mA draw with WiFi off. |

## Software & tools

* Arduino IDE 2.x, or Visual Studio + [VisualMicro](https://www.visualmicro.com/)
* esp32 Arduino core 3.3.x — board *XIAO_ESP32C3*, partition *Default 4MB with spiffs*, *USB CDC On Boot: Enabled*
* [Seeed_GFX](https://github.com/Seeed-Studio/Seeed_GFX) library (git submodule in `firmware/libraries/`; conflicts with TFT_eSPI / Adafruit_GFX)
* `driver.h` in the sketch folder (in the repo)
* SPIFFS upload tool for `Data/bg.png` (see README)
* Orbitron Bold TTF (`assets/fonts/`) + Adafruit `fontconvert`, for regenerating the `Orbitron*pt7b.h` headers
