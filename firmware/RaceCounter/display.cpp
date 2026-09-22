//////////////////////////////////////////////////////////////////////////////
//
// Display module: everything that draws on the ePaper panel - the counter screens (splash, heat banner +
// race number, PRACTICE), the text status screens and the information screen.
//
//////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>

#include "RaceCounter.h"
#include "display.h"
#include "wifi_config.h"
#include "rh_client.h"

#include "RotorHazardLogo.h"
#include "Orbitron50pt7b.h"
#include "Orbitron68pt7b.h"

EPaper epaper;

// Which screen is on the panel, so the information screen can put back whatever it replaced

enum Screen {
    SCREEN_STATUS,          // a text screen drawn during startup (connecting, standalone notice, ...)
    SCREEN_COUNTER,         // the rendered DisplayState: splash, race number or practice
    SCREEN_AP_SETUP,
    SCREEN_INFO
};

static Screen       currentScreen  = SCREEN_STATUS;
static Screen       previousScreen = SCREEN_STATUS;
static DisplayState rendered;                   // what the panel shows while currentScreen == SCREEN_COUNTER


//////////////////////////////////////////////////////////////////////////////
//
// The counter screen. deriveState() turns the race state into what should be on the panel; render() draws
// it, unless it is already there. All the counter-screen entry points below go through these two.
//

static DisplayState deriveState() {
    DisplayState s;

    s.linkLost = rhLinkLost();

    if (practiceMode) {
        s.splash = false;
        s.banner = "PRACTICE";
        s.big    = "P";
    }
    else if (raceCount == 0) {                  // nothing to count yet: the RotorHazard splash screen
        s.splash = true;
    }
    else {
        s.splash = false;
        if (bannerText.length())
            s.banner = bannerText;
        else if (heatCount)
            s.banner = "Heat " + String(heatCount);
        else
            s.banner = "RACE #";
        s.big = String(raceCount);
        s.roundLabel = ROUNDLABEL_ENABLED && s.banner != "RACE #" && raceCount < 20;   // (a two-digit number from 20 up leaves no room for it)
    }
    return s;
}

// Selects the banner font: Orbitron 50pt if the text fits the panel width, otherwise a smaller font so a
// long banner (a RotorHazard heat name, say) still fits on one line.

static void selectBannerFont(const String &text) {
    epaper.setFreeFont(&BANNERFONT);
    epaper.setTextSize(BANNERFONTSIZE);
    if (epaper.textWidth(text.c_str()) <= DISPLAYWIDTH - 10)
        return;

    epaper.setFreeFont(&FreeSansBold24pt7b);
    epaper.setTextSize(2);
    if (epaper.textWidth(text.c_str()) <= DISPLAYWIDTH - 10)
        return;

    epaper.setTextSize(1);                      // (a 40-character banner still won't fit; it just gets clipped)
}

// The "connection to the timer lost" marker: a warning triangle with an exclamation mark, top right.

static void drawLinkLostMarker() {
    const int   size = 44, margin = 6;
    int         x1 = DISPLAYWIDTH - margin - size, x2 = DISPLAYWIDTH - margin, xm = (x1 + x2) / 2;
    int         y1 = margin, y2 = margin + size;

    epaper.fillTriangle(xm, y1, x1, y2, x2, y2, TFT_BLACK);
    epaper.fillTriangle(xm, y1 + 9, x1 + 7, y2 - 4, x2 - 7, y2 - 4, TFT_WHITE);
    epaper.setFreeFont(&FreeSansBold12pt7b);
    epaper.setTextSize(1);
    epaper.setCursor(xm - 3, y2 - 7);
    epaper.print("!");
}

// Draws text reading upwards (rotated 90 degrees anticlockwise) with its left edge at x = left, centred on
// y = centerY, with `gap` pixels between the letters. The text is drawn into a 1-bit sprite the normal way
// and then copied to the panel pixel by pixel with the axes swapped.

static void drawVerticalLabel(const char *text, int left, int centerY, int gap) {
    const GFXfont  *font = &ROUNDLABELFONT;
    const int       size = ROUNDLABELFONTSIZE;
    const int       n = strlen(text);
    int             length = 0, thick = 0, ascent = 0;

    for (int i = 0; i < n; i++) {               // ink widths of the letters, and the tallest of them
        const GFXglyph *g = &font->glyph[text[i] - font->first];
        length += g->width * size + (i ? gap : 0);
        thick   = max(thick, g->height * size);
        ascent  = max(ascent, -g->yOffset * size);
    }

    TFT_eSprite spr(&epaper);
    spr.setColorDepth(1);
    if (!spr.createSprite(length, thick))
        return;
    spr.fillSprite(TFT_WHITE);
    spr.setTextColor(TFT_BLACK);
    spr.setFreeFont(font);
    spr.setTextSize(size);

    for (int i = 0, x = 0; i < n; i++) {
        const GFXglyph *g = &font->glyph[text[i] - font->first];
        spr.setCursor(x - g->xOffset * size, ascent);
        spr.print(text[i]);
        x += g->width * size + gap;
    }

    int bottom = centerY + length / 2;
    for (int sy = 0; sy < thick; sy++)          // sprite x runs up the panel, sprite y runs across it
        for (int sx = 0; sx < length; sx++)
            if (spr.readPixelValue(sx, sy) == 0)
                epaper.drawPixel(left + sy, bottom - sx, TFT_BLACK);

    spr.deleteSprite();
}

static void render(const DisplayState &s, bool force = false) {
    uint16_t    W, H;
    int         x;

    if (!force && currentScreen == SCREEN_COUNTER && s == rendered)
        return;                                 // already showing exactly this

    epaper.fillScreen(TFT_WHITE);

    if (s.splash) {
        epaper.drawBitmap((DISPLAYWIDTH - LOGOWIDTH) / 2, (DISPLAYHEIGHT - LOGOHEIGHT) / 2, RotorHazardLogo, LOGOWIDTH, LOGOHEIGHT, TFT_WHITE, TFT_BLACK);
    }
    else {
        selectBannerFont(s.banner);
        H = epaper.fontHeight();
        W = epaper.textWidth(s.banner.c_str());
        epaper.setCursor((DISPLAYWIDTH - W) / 2, H);                            // center top
        epaper.print(s.banner);

        epaper.setFreeFont(&STATEFONT);
        epaper.setTextSize(STATEFONTSIZE);
        W = epaper.textWidth(s.big.c_str());
        x = (DISPLAYWIDTH - W) / 2 + CENTERINGOFFSET;                           // center bottom

        if (s.roundLabel) {
            const GFXglyph *g = &STATEFONT.glyph[s.big[0] - STATEFONT.first];   // centre the label on the digits' ink box
            drawVerticalLabel("ROUND", s.big.length() == 1 ? ROUNDLABEL_X1 : ROUNDLABEL_X2,
                              DISPLAYHEIGHT - 20 + (g->yOffset + g->height / 2) * STATEFONTSIZE, ROUNDLABEL_LETTER_GAP);
        }

        epaper.setCursor(x, DISPLAYHEIGHT - 20);
        epaper.print(s.big);
    }

    if (s.linkLost)
        drawLinkLostMarker();

    epaper.update();
    rendered = s;
    currentScreen = SCREEN_COUNTER;
}

void updateDisplay() {
    render(deriveState());
}

// Leave practice mode (if in it) and show the heat / race number.

void doRaceCount() {
    practiceMode = false;
    updateDisplay();
}

// Toggle practice mode. Leaving it with nothing counted yet starts at race 1 rather than going back to the splash.

void doPractice() {
    practiceMode = !practiceMode;

    if (!practiceMode && raceCount == 0)
        raceCount = 1;

    updateDisplay();
}

String displayBanner() {
    DisplayState s = deriveState();
    return s.splash ? String("") : s.banner;
}

String displayDescription() {
    switch (currentScreen) {
        case SCREEN_COUNTER:    return rendered.splash ? String("RotorHazard splash screen") : rendered.banner + " / " + rendered.big;
        case SCREEN_AP_SETUP:   return "WiFi setup instructions";
        case SCREEN_INFO:       return "information screen";
        default:                return "startup text";
    }
}

const char *displayScreenName() {
    switch (currentScreen) {
        case SCREEN_COUNTER:    return rendered.splash ? "splash" : (practiceMode ? "practice" : "counter");
        case SCREEN_AP_SETUP:   return "ap_setup";
        case SCREEN_INFO:       return "info";
        default:                return "status";
    }
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

    epaper.print("\nRotorHazard: ");
    if (rhConfigured()) {
        epaper.print(rhServer);
        epaper.print(" - ");
    }
    epaper.println(rhStatusText());
    if (rhHeatId() >= 0) {
        snprintf(buf, sizeof(buf), "Timer heat: %s, next round %d", rhHeatId() ? rhHeatName().c_str() : "(practice)", rhRound());
        epaper.println(buf);
    }

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
    if (previousScreen == SCREEN_AP_SETUP)
        drawApSetupScreen();
    else
        render(deriveState(), true);            // the counter screen (a startup status screen has nothing to go back to, so it becomes the counter screen)
}
