//////////////////////////////////////////////////////////////////////////////
//
// RotorHazard Race Counter - ePaper heat / race number display for the flight line.
// Hardware, housing and original firmware by RocketSled. See README.md and RaceCounter.h for the source layout.
//
//////////////////////////////////////////////////////////////////////////////

#include "RaceCounter.h"
#include "display.h"
#include "web.h"
#include "wifi_config.h"
#include "buttons.h"

// Shared race state (declared in RaceCounter.h)

uint8_t raceCount = 0;
uint8_t heatCount = 0;
bool    practiceMode = false;


//////////////////////////////////////////////////////////////////////////////
//
// Setup
//

void setup()
{
    Serial.begin(115200);

    delay(2000);

    Serial.printf("\n\n\n------" PAGE_TITLE " v" FW_VERSION "-----\n");

    initButtons();

    epaper.begin();

    checkClearAll();                        // check if both buttons are pressed on startup, if so, does not return, clears WiFi credentials and restarts
                                            // which then comes back through setup again, but with cleared credentials 

    loadSettings();

    if (standAlone == "TRUE") {
        Serial.println("Standalone mode enabled, skipping WiFi setup");
        startStatusScreen();
        epaper.print("\nStandalone mode enabled.\nNo WiFi needed to operate.\nPush both buttons on startup\nto clear this setting.\n");
        epaper.update();
        delay(2500);
        standAloneMode = true;
        doWelcomeScreen();
        return;                             // skip all the WiFi stuff and just start the display for manual use only
    }

    if (ssidStored == "") {                 // go to AP mode instead of trying to connect to WiFi
        inAPMode = true;                    // in AP mode the buttons only set standalone mode or show the information screen
        startConfigAP();                    // (draws the setup screen)
        return;
    }

    inAPMode = false;

    connectToWiFi();
    startWebServer();

    delay(2000);

    doWelcomeScreen();
}


//////////////////////////////////////////////////////////////////////////////
//
// Main loop
//

void loop()
{
    server.handleClient();
    handleButtons();
}
