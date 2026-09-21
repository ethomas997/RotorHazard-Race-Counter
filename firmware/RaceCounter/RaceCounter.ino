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
    uint16_t    H;

    char        charBuff[128];

    Serial.begin(115200);

    delay(2000);

    Serial.printf("\n\n\n------" PAGE_TITLE " v" FW_VERSION "-----\n");

    initButtons();

    epaper.begin();
    epaper.fillScreen(TFT_WHITE);
    epaper.setFreeFont(&FreeSansBold12pt7b);
    epaper.setTextSize(1);
    H = epaper.fontHeight();

	checkClearAll(H);                       // check if both buttons are pressed on startup, if so, does not return, clears WiFi credentials and restarts
	                                        // which then comes back through setup again, but with cleared credentials

    loadSettings();

	if (standAlone == "TRUE") {
		Serial.println("Standalone mode enabled, skipping WiFi setup");
		sprintf(charBuff, "Standalone mode enabled.\nNo WiFi needed to operate.\nPush both buttons on startup\nto clear this setting.\n");
		epaper.setCursor(0, H);              // center top
		epaper.print(charBuff);
		epaper.update();
        delay(2500);
		doWelcomeScreen();
		standAloneMode = true;
		return;                             // skip all the WiFi stuff and just start the display for manual use only
	}

    if (ssidStored == "") {                 // go to AP mode instead of trying to connect to WiFi
        sprintf(charBuff, "No stored WiFi credentials.\nConnect to SSID:\n   RaceCounter-Setup\nto configure...\n" );
        epaper.setCursor(0, H);              // center top
        epaper.print(charBuff);
		epaper.println("Push both buttons to set standalone mode");
        epaper.update();
        startConfigAP();
		inAPMode = true;                    // in AP mode, loop() checks for both buttons pressed, and turns on stand alone mode so no WiFI needed to operate
        return;
    }

	inAPMode = false;

    connectToWiFi(H);
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
