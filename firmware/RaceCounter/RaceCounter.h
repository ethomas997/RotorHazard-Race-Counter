//////////////////////////////////////////////////////////////////////////////
//
// RotorHazard Race Counter - configuration and shared state.
//
// Source layout (the Arduino IDE shows these as tabs):
//   RaceCounter.ino     - setup() and loop()
//   RaceCounter.h       - this file: #defines and the shared race state
//   display.h / .cpp    - drawing on the ePaper panel
//   web.h / .cpp        - web interface and HTTP API
//   wifi_config.h / .cpp- WiFi connection, setup access point, saved credentials, standalone mode
//   buttons.h / .cpp    - DN / UP push-button handling
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Arduino.h>

#define FW_VERSION  "3.0.4"                     // shown on the settings page and in the serial banner
#define PAGE_TITLE  "RotorHazard Race Counter"  // web page titles / headings

#define DNSWITCH    D5
#define UPSWITCH    D6

#define DISPLAYWIDTH 648
#define DISPLAYHEIGHT 480

#define BANNERFONT      Orbitron50pt7b
#define BANNERFONTSIZE  1
#define STATEFONT       Orbitron68pt7b
#define STATEFONTSIZE   3
#define HEATFONT        Orbitron50pt7b
#define HEATFONTSIZE    1
#define ROUNDLABEL_ENABLED  true    // show a vertical "ROUND" label beside race numbers 1-19 when the banner isn't "RACE #"
#define ROUNDLABELFONT      FreeSansBold18pt7b   // the vertical "ROUND" label beside the race number (Orbitron's D and O look alike this small)
#define ROUNDLABELFONTSIZE  1
#define ROUNDLABEL_X1       22      // x of the label's left edge beside a single-digit number
#define ROUNDLABEL_X2       11      // ... and beside a two-digit number (10-19)
#define ROUNDLABEL_LETTER_GAP 18    // space between the label's letters (the label is centred on the number's height)

#define CENTERINGOFFSET -10     // the width of text returned by the library is saying the text is slightly wider than it actually is, so it doesn't center right. This compensates.

#define CONNECT_POLL_MS     300     // how often to check for a WiFi connection (and the both-buttons escape) while connecting
#define CONNECT_REFRESH_MS  10000   // how often to refresh the ePaper with progress dots while connecting

#define BUTTON_DEBOUNCE_MS  20      // a button reading has to hold this long before it counts (filters contact bounce)
#define BUTTON_COMBO_MS     250     // after one button goes down, how long to wait for the other before treating it as a single press
#define BUTTON_LONG_MS      3000    // how long both buttons must be held to bring up the information screen
#define BUTTON_REPEAT_MS    300     // minimum time between steps while a single button is held (the panel refresh is usually longer)

// Shared race state (defined in RaceCounter.ino)

extern uint8_t  raceCount;          // 1..99; 0 while the splash screen is showing
extern uint8_t  heatCount;          // 0 = no heat number shown
extern bool     practiceMode;
extern String   bannerText;         // banner override (e.g. a RotorHazard heat name); "" = derive it from heatCount
