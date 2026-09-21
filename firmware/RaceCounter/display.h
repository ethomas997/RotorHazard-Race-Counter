//////////////////////////////////////////////////////////////////////////////
//
// Display module: everything that draws on the ePaper panel.
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

void doPractice();          // toggle practice mode and draw the PRACTICE screen (or the race count if leaving practice)
void doRaceCount();         // draw the heat banner and the race number
void doWelcomeScreen();     // draw the RotorHazard splash screen
