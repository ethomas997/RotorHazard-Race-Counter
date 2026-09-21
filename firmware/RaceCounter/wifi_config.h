//////////////////////////////////////////////////////////////////////////////
//
// WiFi / configuration module: connecting to the stored network, the RaceCounter-Setup access point and
// its setup page, saving / clearing credentials in Preferences, and standalone mode.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Arduino.h>

extern String   ssidStored;         // saved WiFi credentials ("" = none saved)
extern String   passStored;
extern String   standAlone;         // "TRUE" when standalone mode (no WiFi) has been selected
extern bool     inAPMode;           // running the RaceCounter-Setup access point
extern bool     standAloneMode;     // running without WiFi

void   loadSettings();              // read the saved credentials and standalone flag from Preferences
String stationMacAddress();         // the MAC the router sees (also correct while in AP mode)
void   connectToWiFi(uint16_t H);   // connect to the stored network; does not return until connected
void   startConfigAP();             // start the setup access point and its web page
void   checkClearAll(uint8_t H);    // if both buttons are down: erase credentials + standalone flag and restart
void   setStandAlone();             // set standalone mode and restart

// Route handlers shared with the main web server (registered in startWebServer())
void   handleSaveAP();              // POST /save  - store new SSID / password and reboot
void   handleReset();               // POST /reset - erase the stored credentials and reboot into AP mode
