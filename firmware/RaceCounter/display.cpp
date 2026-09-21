//////////////////////////////////////////////////////////////////////////////
//
// Display module: everything that draws on the ePaper panel - the race count / heat banner, the PRACTICE
// screen, the RotorHazard splash screen, the text status screens and the information screen.
//
//////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>

#include "RaceCounter.h"
#include "display.h"
#include "wifi_config.h"

#include "RotorHazardLogo.h"
#include "Orbitron50pt7b.h"
#include "Orbitron68pt7b.h"

EPaper epaper;

// Which screen is on the panel, so the information screen can put back whatever it replaced

enum Screen {
    SCREEN_STATUS,          // a text screen drawn during startup (connecting, standalone notice, ...)
    SCREEN_SPLASH,
    SCREEN_RACE,
    SCREEN_PRACTICE,
    SCREEN_AP_SETUP,
    SCREEN_INFO
};

static Screen currentScreen  = SCREEN_STATUS;
static Screen previousScreen = SCREEN_STATUS;


//////////////////////////////////////////////////////////////////////////////
//
// Handler for the Practice button. Toggles practice mode on and off. In practice mode, the display shows "PRACTICE" at the top and "P" at the bottom.
//

void doPractice() {
    practiceMode = !practiceMode;

    if (practiceMode == false ) {
        if (raceCount == 0)
            raceCount = 1;
        doRaceCount();
        return;
    }

    drawPracticeScreen();
}

void drawPracticeScreen() {
    uint16_t    W, H;

    char    charBuff[128];

    epaper.fillScreen(TFT_WHITE);
    epaper.setFreeFont(&BANNERFONT);
    epaper.setTextSize(BANNERFONTSIZE);
    H = epaper.fontHeight();

    sprintf(charBuff, "PRACTICE");
    W = epaper.textWidth(charBuff);

    epaper.setCursor((DISPLAYWIDTH - W) / 2, H);              // center top
    epaper.print(charBuff);

    epaper.setFreeFont(&STATEFONT);
    epaper.setTextSize(STATEFONTSIZE);

    W = epaper.textWidth("P");
    H = epaper.fontHeight();

    epaper.setCursor((DISPLAYWIDTH - W) / 2, DISPLAYHEIGHT - 20);        // center bottom
    epaper.printf("P");

    epaper.update();
    currentScreen = SCREEN_PRACTICE;
}


//////////////////////////////////////////////////////////////////////////////
//
// handler for displaying the race count.
//


void    doRaceCount() {
    uint16_t    W, H;

    char        charBuff[128];

    practiceMode = false;

    epaper.fillScreen(TFT_WHITE);
    epaper.setFreeFont(&BANNERFONT);
    epaper.setTextSize(BANNERFONTSIZE);
    H = epaper.fontHeight();

    if (heatCount == 0) {
        epaper.setFreeFont(&BANNERFONT);
        epaper.setTextSize(BANNERFONTSIZE);
        H = epaper.fontHeight();

        sprintf(charBuff, "RACE #");
    }
    else {
        epaper.setFreeFont(&HEATFONT);
        epaper.setTextSize(HEATFONTSIZE);
        H = epaper.fontHeight();

        sprintf(charBuff, "Heat %d", heatCount);
    }

    W = epaper.textWidth(charBuff);
    epaper.setCursor((DISPLAYWIDTH - W) / 2, H);              // center top
    epaper.print(charBuff);

    epaper.setFreeFont(&STATEFONT);
    epaper.setTextSize(STATEFONTSIZE);

    sprintf(charBuff, "%d", raceCount);
    W = epaper.textWidth(charBuff);
    H = epaper.fontHeight();

    epaper.setCursor((DISPLAYWIDTH - W) / 2 + CENTERINGOFFSET, DISPLAYHEIGHT - 20);        // center bottom
    epaper.printf("%s", charBuff);

    epaper.update();
    currentScreen = SCREEN_RACE;
}


//////////////////////////////////////////////////////////////////////////////
//
// handler for displaying the RotorHazard spalsh screen
//

void doWelcomeScreen() {
    epaper.fillScreen(TFT_WHITE);
    epaper.drawBitmap((DISPLAYWIDTH - LOGOWIDTH) / 2, (DISPLAYHEIGHT - LOGOHEIGHT) / 2, RotorHazardLogo, LOGOWIDTH, LOGOHEIGHT, TFT_WHITE, TFT_BLACK);
    epaper.update();
    currentScreen = SCREEN_SPLASH;
}


//////////////////////////////////////////////////////////////////////////////
//
// Text screens. startStatusScreen() clears the panel, selects the small font and prints the name and
// firmware version on the first line, leaving the cursor at the start of the next line. The caller then
// prints its own text and calls epaper.update().
//

void startStatusScreen() {
    epaper.fillScreen(TFT_WHITE);
    epaper.setFreeFont(&FreeSansBold12pt7b);
    epaper.setTextSize(1);
    epaper.setCursor(0, epaper.fontHeight());
    epaper.println(PAGE_TITLE " v" FW_VERSION);
    currentScreen = SCREEN_STATUS;
}

// The screen shown while running the RaceCounter-Setup access point. Can be redrawn at any time (the
// information screen puts it back), so it's drawn from the live AP state rather than from setup().

void drawApSetupScreen() {
    startStatusScreen();
    epaper.print("\nNo stored WiFi credentials.\nConnect to SSID:\n   RaceCounter-Setup\nto configure...\n");
    epaper.print("\nAP IP: ");
    epaper.println(WiFi.softAPIP());
    epaper.print("MAC: ");
    epaper.println(stationMacAddress());
    epaper.println("\nPush both buttons to set standalone mode");
    epaper.update();
    currentScreen = SCREEN_AP_SETUP;
}


//////////////////////////////////////////////////////////////////////////////
//
// The information screen, shown by holding both buttons. Any button press afterwards restores the screen
// that was showing before (see handleButtons()).
//

void showInfoScreen() {
    char    buf[64];

    if (currentScreen != SCREEN_INFO)           // (a second showInfoScreen() must not remember the info screen as "previous")
        previousScreen = currentScreen;

    startStatusScreen();

    epaper.setFreeFont(&FreeSans12pt7b);

    if (standAloneMode) {
        epaper.println("\nMode: standalone (no WiFi)");
    }
    else if (inAPMode) {
        epaper.println("\nMode: WiFi setup access point");
        epaper.println("SSID: RaceCounter-Setup");
        epaper.print("IP: ");
        epaper.println(WiFi.softAPIP());
    }
    else {
        epaper.println("\nMode: WiFi client");
        epaper.print("SSID: ");
        epaper.println(ssidStored);
        if (WiFi.status() == WL_CONNECTED) {
            epaper.print("IP: ");
            epaper.println(WiFi.localIP());
            epaper.print("Web page: http://");
            epaper.print(WiFi.localIP());
            epaper.println("/");
            snprintf(buf, sizeof(buf), "Signal: %d dBm", WiFi.RSSI());
            epaper.println(buf);
        }
        else {
            epaper.println("IP: (not connected)");
        }
    }
    epaper.print("MAC: ");
    epaper.println(stationMacAddress());

    epaper.println();
    if (practiceMode)
        strcpy(buf, "Showing: practice mode");
    else if (raceCount == 0)
        strcpy(buf, "Showing: splash screen");
    else if (heatCount)
        snprintf(buf, sizeof(buf), "Showing: heat %d, race %d", heatCount, raceCount);
    else
        snprintf(buf, sizeof(buf), "Showing: race %d", raceCount);
    epaper.println(buf);

    unsigned long secs = millis() / 1000;
    snprintf(buf, sizeof(buf), "Up: %luh %02lum %02lus", secs / 3600, (secs / 60) % 60, secs % 60);
    epaper.println(buf);
    snprintf(buf, sizeof(buf), "Free memory: %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
    epaper.println(buf);

    epaper.println("\nPress any button to return");
    epaper.update();
    currentScreen = SCREEN_INFO;
}

bool infoScreenShowing() {
    return currentScreen == SCREEN_INFO;
}

void restorePreviousScreen() {
    switch (previousScreen) {
        case SCREEN_RACE:       doRaceCount();          break;
        case SCREEN_PRACTICE:   drawPracticeScreen();   break;
        case SCREEN_AP_SETUP:   drawApSetupScreen();    break;
        case SCREEN_SPLASH:
        default:                doWelcomeScreen();      break;      // a startup status screen has nothing to go back to
    }
}
