//////////////////////////////////////////////////////////////////////////////
//
// Display module: everything that draws on the ePaper panel.
//
// The counter screens (splash / heat + race number / practice) are all rendered from one DisplayState that
// is derived from the shared race state, and the panel is only refreshed when that state actually changes -
// an ePaper refresh takes seconds, so redrawing the same thing is never free.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

// The hardware requires the Seeed_GFX library, a fork of TFT_eSPI, which reads driver.h in this folder to
// select the panel and driver board (see README). If EPAPER_ENABLE isn't defined after including it, the
// wrong library is installed or driver.h wasn't found - the sketch would compile against plain TFT_eSPI
// but never drive the panel, so stop here instead.
#include "TFT_eSPI.h"

#ifndef EPAPER_ENABLE
#error "Seeed_GFX ePaper support not enabled: install Seeed_GFX (not TFT_eSPI / Adafruit_GFX) and keep driver.h in the sketch folder"
#endif

extern EPaper epaper;

// What the counter screen shows: either the splash logo, or a banner line over a big text (the race number,
// or "P" in practice mode). The banner is free text, so it can be a RotorHazard heat name.

struct DisplayState {
    bool    splash = true;
    String  banner;
    String  big;
    bool    roundLabel = false; // show a vertical "ROUND" label to the left of the big text (a race number)
    bool    linkLost = false;   // show the "connection to the timer lost" marker

    bool operator==(const DisplayState &o) const { return splash == o.splash && banner == o.banner && big == o.big && roundLabel == o.roundLabel && linkLost == o.linkLost; }
    bool operator!=(const DisplayState &o) const { return !(*this == o); }
};

// The counter screens
void updateDisplay();           // derive the DisplayState from the race state and redraw the panel if it changed
void doRaceCount();             // leave practice mode (if in it) and show the heat / race number
void doPractice();              // toggle practice mode and show the result
String displayBanner();         // the banner text currently derived for the counter screen ("" on the splash screen)
const char *displayScreenName();// "splash", "counter", "practice", "info", "ap_setup" or "status" - what the panel shows
String displayDescription();    // what the panel shows, for people: "Heat 1 / 17", "PRACTICE / P", "RotorHazard splash screen", ...

// Text screens (small font). startStatusScreen() clears the panel, prints the name and firmware version on
// the first line and leaves the cursor on the next line; the caller prints its text and calls epaper.update().
void startStatusScreen();
void drawApSetupScreen();       // the "connect to RaceCounter-Setup" screen shown while running the setup access point

// The information screen (long press of both buttons)
void showInfoScreen();          // show device information, remembering which screen was showing before
bool infoScreenShowing();
void restorePreviousScreen();   // redraw whatever was showing before the information screen
