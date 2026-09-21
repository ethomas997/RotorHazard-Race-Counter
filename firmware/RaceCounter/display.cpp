//////////////////////////////////////////////////////////////////////////////
//
// Display module: everything that draws on the ePaper panel - the race count / heat banner, the PRACTICE
// screen and the RotorHazard splash screen.
//
//////////////////////////////////////////////////////////////////////////////

#include "RaceCounter.h"
#include "display.h"

#include "RotorHazardLogo.h"
#include "Orbitron50pt7b.h"
#include "Orbitron68pt7b.h"

EPaper epaper;


//////////////////////////////////////////////////////////////////////////////
//
// Handler for the Practice button. Toggles practice mode on and off. In practice mode, the display shows "PRACTICE" at the top and "P" at the bottom. 
//

void doPractice() {

    uint16_t    W, H;

    char    charBuff[128];

    practiceMode = !practiceMode;

    if (practiceMode == false ) {
        if (raceCount == 0)
            raceCount = 1;
        doRaceCount();
        return;
    }

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
}


//////////////////////////////////////////////////////////////////////////////
//
// handler for displaying the RotorHazard spalsh screen
//

void doWelcomeScreen() {
    epaper.fillScreen(TFT_WHITE);
    epaper.drawBitmap((DISPLAYWIDTH - LOGOWIDTH) / 2, (DISPLAYHEIGHT - LOGOHEIGHT) / 2, RotorHazardLogo, LOGOWIDTH, LOGOHEIGHT, TFT_WHITE, TFT_BLACK);
    epaper.update();
}
