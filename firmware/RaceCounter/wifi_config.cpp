//////////////////////////////////////////////////////////////////////////////
//
// WiFi / configuration module: connecting to the stored network, the RaceCounter-Setup access point and its
// setup page, saving / clearing credentials in Preferences, and standalone mode.
//
//////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <Preferences.h>
#include <esp_mac.h>

#include "RaceCounter.h"
#include "display.h"
#include "web.h"
#include "wifi_config.h"

String  ssidStored = "";
String  passStored = "";
String  standAlone = "";
bool    inAPMode = false;
bool    standAloneMode = false;

static Preferences prefs;

static void handleRootAP();


//////////////////////////////////////////////////////////////////////////////
//
// Returns the station (client) MAC address as a string. This is the address the router sees when the device
// connects to a WiFi network, so it's the one to use for a static DHCP lease. Read from the chip directly
// (rather than WiFi.macAddress()) so it is also correct while running as an access point, where the station
// interface doesn't exist yet. Note the soft-AP MAC is different (station MAC + 1 in the last octet).
//

String stationMacAddress() {
    uint8_t mac[6];
    char    buf[18];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}


//////////////////////////////////////////////////////////////////////////////
//
// Connects to the stored WiFi network, showing progress on the panel, then shows the IP address and MAC.
// Does not return until connected; holding both buttons while it waits clears the credentials and restarts
// (see checkClearAll).
//
//////////////////////////////////////////////////////////////////////////////

void connectToWiFi() {
    char charBuff[128];

    sprintf(charBuff, "Connecting to SSID: %s\n", ssidStored.c_str());
    Serial.print(charBuff);

    startStatusScreen();
    epaper.print(charBuff);
    epaper.update();

    WiFi.begin(ssidStored, passStored);

    epaper.setFreeFont(&FreeSans12pt7b);

    unsigned long lastRefresh = millis();

    while (WiFi.status() != WL_CONNECTED) {
        delay(CONNECT_POLL_MS);
        Serial.print(".");
        epaper.print(".");                                      // dots accumulate in the frame buffer...
        if (millis() - lastRefresh >= CONNECT_REFRESH_MS) {     // ...but a full ePaper refresh takes seconds and wears the panel, so only push them out occasionally
            epaper.update();
            lastRefresh = millis();
        }
        checkClearAll();
    }

    Serial.println("\nConnected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC: ");
    Serial.println(stationMacAddress());

    epaper.print("...Connected!\n");

    epaper.setFreeFont(&FreeSansBold12pt7b);
    epaper.print("\nIPAddress: ");
    epaper.println(WiFi.localIP());
    epaper.print("MAC: ");
    epaper.println(stationMacAddress());
    epaper.update();
}


//////////////////////////////////////////////////////////////////////////////
//
// This function is called at startup if there is no stored WiFi SSID and password.
// It sets up the device as a WiFi access point, displays the AP information on the screen, and starts the web server to handle configuration requests.
//

void startConfigAP() {
    Serial.println("Starting WiFi setup AP...");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("RaceCounter-Setup");

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    drawApSetupScreen();

    server.on("/", handleRootAP);
    server.on("/save", HTTP_POST, handleSaveAP);
    server.begin();
}


//////////////////////////////////////////////////////////////////////////////
//
// This handler is only invoked when the device is in AP mode, which happens when there is no stored WiFi SSID and password. 
// It generates the HTML for the page that allows you to enter a WiFi SSID and password, and submit them to be saved in preferences.
//

static void handleRootAP() {
    String page = "<!DOCTYPE html><html><head><title>" PAGE_TITLE "</title>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>";
    page += "<style>";
    page += "h1{font-size:26px;margin:8px 0 12px 0;font-weight:bold;}";
    page += "h2{font-size:20px;margin:4px 0;font-weight:bold;}";
    page += "h3{font-size:18px;margin:4px 0;font-weight:bold;}";
    page += "h4{font-size:16px;margin:3px 0;font-weight:bold;}";

    page += "button{font-size:20px;height:40px;margin:2px;padding:0 10px;border-radius:6px;border:1px solid #666;background:#e0e0e0;}";
    page += "button:active{background:#ccc;}";

    page += "</style></head><body style='font-family:sans-serif;'>";

    page += "<h2>" PAGE_TITLE " WiFi Setup</h2>";
    page += "<form action='/save' method='POST'>";
    page += "SSID:<br><input name='ssid'><br><br>";
    page += "Password:<br><input name='pass' type='password'><br><br>";
    page += "<button type='submit'>Save & Reboot</button>";
    page += "</form><br>Push both buttons to set standalone mode</body></html>";
    server.send(200, "text/html", page);
}


//////////////////////////////////////////////////////////////////////////////
//
// This handler is only invoked when the device is in AP mode, which happens when there is no stored WiFi SSID and password.
// It handles the saving of the SSID and password entered by the user on the RootAP page.
// It checks that both SSID and password are provided, saves them in preferences, shows a confirmation message, and restarts the device after a short delay.
//

void handleSaveAP() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");

        prefs.begin("wifi", false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        prefs.end();

        String message = "<html><body><h2>";
        message += "new SSID: ";
        message += ssid;
        message += "<br>PWD is: ";
        message += pass;
        message += "<br><br>Saved!<br>Rebooting...</h2></body></html>";

        server.send(200, "text/html", message);

        delay(1500);
        ESP.restart();
    }
    else {
        server.send(400, "text/plain", "Missing SSID or password");
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// the settings page "Clear" button (POST /reset). This clears the stored WiFi SSID and password, shows a message on the display, and restarts the device after a delay.
//

void handleReset() {
    prefs.begin("wifi", false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();

    server.send(200, "text/html", "<html><body style='font-family:sans-serif;'><h2>WiFi credentials cleared.<br>Rebooting into setup mode...</h2></body></html>");

    startStatusScreen();
    epaper.print("\nOld SSID and password deleted\nRestarting in 5 seconds...");
    epaper.update();
    delay(5000);

    heatCount = 0;
    raceCount = 1;
    practiceMode = false;

    ESP.restart();
}


//////////////////////////////////////////////////////////////////////////////
//
// This handler is only invoked when the device is in AP mode, which happens when there is no stored WiFi SSID and password.
// it sets standalone mode and triggers a restart, which will then skip all the wifi stuff and start the display for manual use only
//

void setStandAlone() {
    prefs.begin("SA", false);
    prefs.putString("SA", "TRUE");
    prefs.end();
	Serial.println("Standalone mode set, restarting...");
    delay(2500);
    ESP.restart();
}


//////////////////////////////////////////////////////////////////////////////
//
// There are a couple of places where pushing both buttons on the display unit at the same time triggers a reset of the WiFi credentials. 
// Checked immediately on startup, and during the Wifi connection loop (so the loop can be aborted if the credentials turn out to be no good).
// This function is called in those places and if both buttons are presseed, it clears the stored SSID and password, shows a message on the display, and restarts after a delay.
//

void checkClearAll() {
    if ((digitalRead(UPSWITCH) == LOW) && (digitalRead(DNSWITCH) == LOW)) {
        Serial.println("Clearing SSID and Password, then restarting");

        prefs.begin("wifi", false);
        prefs.remove("ssid");
        prefs.remove("pass");
        prefs.end();

        prefs.begin("SA", false);              // standalone mode
        prefs.remove("SA");
        prefs.end();

        startStatusScreen();
        epaper.print("\nOld SSID and password deleted\nStandalone disabled\nRestarting in 5 seconds");
        epaper.update();
        delay(5000);
        ESP.restart();
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// Reads the saved WiFi credentials and the standalone flag from Preferences.
//

void loadSettings() {
    prefs.begin("wifi", true);
    ssidStored = prefs.getString("ssid", "");
    passStored = prefs.getString("pass", "");
    prefs.end();

    prefs.begin("SA", true);                // standalone mode
    standAlone = prefs.getString("SA", "");
    prefs.end();
}
