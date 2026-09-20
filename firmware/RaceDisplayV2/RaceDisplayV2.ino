
#include "Arduino.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>


#include "TFT_eSPI.h"
#include "RotorHazardLogo.h"
#include "Orbitron50pt7b.h"
#include "Orbitron56pt7b.h"
#include "Orbitron68pt7b.h"

#define DNSWITCH    D5
#define UPSWITCH    D6
#define WAKEUPPIN   10

#define DISPLAYWIDTH 648
#define DISPLAYHEIGHT 480

#define BANNERFONT      Orbitron50pt7b
#define BANNERFONTSIZE  1
#define STATEFONT       Orbitron68pt7b
#define STATEFONTSIZE   3
#define HEATFONT        Orbitron50pt7b
#define HEATFONTSIZE    1

#define CENTERINGOFFSET -10     // the width of text returned by the library is saying the text is slightly wider than it actually is, so it doesn't center right. This compensates.

#ifdef EPAPER_ENABLE    // Only compile this code if EPAPER_ENABLE is defined (in User_Setup.h, typically part of the TFT_eSPI library 
                        // Except..., the HW used for this display requires the Seeed_GFX library, a fork of the Adafruit_GFX library, which replaces the TFT_eSPI library 
						// You have to remove the Adafruit_GFX library and replace it with the Seeed_GFX library, since the Seeed library emulates the Adafruit library but adds extra functionality for the ePaper display.
EPaper epaper;
#endif

WebServer   server(80);

uint8_t raceCount;
bool    practiceMode;
bool    justPoweredUp;
bool    LEDToggle;
bool	inAPMode;
bool    standAloneMode;

String  textField = "";
uint8_t heatCount = 0;

Preferences prefs;

String ssidStored = "";
String passStored = "";
String standAlone = "";


//////////////////////////////////////////////////////////////////////////////
//
// Handler for the Practice button. Toggles practice mode on and off. In practice mode, the display shows "PRACTICE" at the top and "P" at the bottom. 
//

void doPractice() {

    int16_t     X, Y;
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
    int16_t     X, Y;
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

        sprintf(charBuff, "HEAT #%d", heatCount);
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


//////////////////////////////////////////////////////////////////////////////
//
// Generate the HTML for the main page, which includes the current heat and race count and buttons to increment/decrement/set them, as well as the Practice and Reset buttons.
//

String handleMainPage() {
    String page = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";

    page += "<style>";

    page += "body{font-family:sans-serif;margin:0;padding:0;background:#fafafa;background:url('/bg.bmp') no-repeat center center fixed;background-size:cover;}";
    page += ".main-container{max-width:480px;min-height:320px;margin:0 auto;padding:10px;}";

    /* Headings */
    page += "h1{font-size:26px;margin:8px 0 12px 0;font-weight:bold;text-align:center;}";
    page += "h2{font-size:20px;margin:4px 0;font-weight:bold;}";
    page += "h3{font-size:18px;margin:4px 0;font-weight:bold;}";
    page += "h4{font-size:16px;margin:3px 0;font-weight:bold;}";

    /* Table layout */
    page += "table{border-collapse:collapse;width:100%;}";
    page += "td{padding:4px;height:50px;vertical-align:middle;}";

    /* Column widths */
    page += "col.inputcol{width:60px;}";
    page += "col.setcol{width:100px;}";
    page += "col.smallcol{width:50px;}";
    page += "col.textcol{width:40px;}";

    /* Inputs */
    page += "input{font-size:24px;width:60px;height:34px;text-align:center;margin:2px;padding:2px;border:2px solid #666;border-radius:4px;}";

    /* Buttons */
    page += "button{font-size:20px;height:40px;margin:2px;padding:0 10px;border-radius:6px;border:2px solid #666;background:#e0e0e0;}";
    page += "button:active{background:#ccc;}";

    /* Increment/Decrement buttons */
    page += ".incdec{width:40px;height:40px;font-size:24px;border-radius:6px;border:2px solid #666;display:flex;justify-content:center;align-items:center;background:#ddd;}";

    /* Practice button states */
    page += ".practiceOn{width:120px;background:#00ff00;color:black;border:2px solid #666;}";
    page += ".practiceOff{width:120px;color:green;font-weight:600;background:#f0f0f0;border:2px solid #0a0;}";
    page += ".resetBut{width:120px;color:red;font-weight:600;background:#f0f0f0;border:2px solid #a00;}";

    /* Bottom button row */
    page += ".bottomButtons{display:flex;gap:20px;justify-content:center;margin-top:10px;}";

    /* Settings icon */
    page += ".settingsIcon{position:fixed;top:10px;right:10px;font-size:32px;cursor:pointer;text-decoration:none;color:#333;}";

    page += ".bgImg{"
        "position:fixed;"
        "top:3%; left:3%;"
        "width:94%; height:94%;"
        "object-fit:contain;"
        "opacity:0.20;"      // adjust fade here
        "z-index:-1;"
        "}";

    page += "</style>";

    page += "</head>";

    page += "<body><img src='/bg.png' class='bgImg'><div class='main-container'>";

    page += "<a href='/settings' class='settingsIcon'>&#128736;</a>";

    page += "<h1>Race Count Display</h1>";

    page += "<table>";
    page += "<colgroup>";
    page += "<col class='textcol'>";
    page += "<col class='smallcol'>";
    page += "<col class='smallcol'>";
    page += "<col class='inputcol'>";
    page += "<col class='setcol'>";
    page += "</colgroup>";

    // -------- ROW 1: Value 1 --------
    page += "<tr>";
    page += "<td><h2>Heat </h2></td>";
    // Decrement button
    page += "<td>";
    page += "<button class='incdec' onclick=\"location.href='/heatDec'\">-</button>";
    page += "</td>";
    // Increment button
    page += "<td>";
    page += "<button class='incdec' onclick=\"location.href='/heatInc'\">+</button>";
    page += "</td>";
    // Value 1 input
    page += "<td>";
    page += "<form action='/heatSet' method='GET'>";

    if (heatCount)
        page += "<input type='number' name='v1' min='0' max='99' value='" + String(heatCount) + "'>";
    else
        page += "<input type='text' value='None' readonly>";

    page += "</td>";
    // Set Value 1 button
    page += "<td>";
    page += "<button type='submit'>Set</button>";
    page += "</form>";
    page += "</td>";
    page += "</tr>";

    // -------- ROW 2: Value 2 --------
    page += "<tr>";
    page += "<td><h2>Race </h2></td>";
    // Decrement button
    page += "<td>";
    page += "<button class='incdec' onclick=\"location.href='/raceDec'\">-</button>";
    page += "</td>";
    // Increment button
    page += "<td>";
    page += "<button class='incdec' onclick=\"location.href='/raceInc'\">+</button>";
    page += "</td>";
    // Value 2 input
    page += "<td>";
    page += "<form action='/raceSet' method='GET'>";
    if (raceCount)
        page += "<input type='number' name='v2' min='1' max='99' value='" + String(raceCount) + "'>";
    else
        page += "<input type='text' value='None' readonly>";

    page += "</td>";
    // Set Value 2 button
    page += "<td>";
    page += "<button type='submit'>Set</button>";
    page += "</form>";
    page += "</td>";
    page += "</tr>";
    page += "</table><br>";

    // Other buttons

    String practiceClass = practiceMode ? "practiceOn" : "practiceOff";

    page += "<div class='bottomButtons'>";
    page += "<button class='" + practiceClass + "' onclick=\"location.href='/practice'\">Practice</button>";
    page += "<button class='resetBut' onclick=\"location.href='/splash'\">Reset</button>";
    page += "</div>";
    page += "<br>";

    page += "</div></body></html>";


    return page;
}


//////////////////////////////////////////////////////////////////////////////
//
// Handle HTML requests for the main page, which shows the current heat and race count stuff, and buttons to change them, as well as the Practice and Reset buttons.
//

void handleRoot() {
    server.send(200, "text/html", handleMainPage());
}


//////////////////////////////////////////////////////////////////////////////
//
// ESP32 leaves the string for the item that was clicked in the browser address bar, so refresh makes it do the same thing again
// Calling this routine at the end of each response throws the browser back to the root page again
//

void handleBackToRoot() {
    server.sendHeader("Location", "/");
    server.send(303);
}


//////////////////////////////////////////////////////////////////////////////
//
// the Race # increment button
//

void handleIncrementRace() {
    raceCount = (raceCount + 1) % 100;   // keep 2‑digit wraparound

    if (raceCount == 0)
        raceCount = 1;

    handleBackToRoot();
    doRaceCount();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Race # decrement button
//

void handleDecrementRace() {
    raceCount = (raceCount - 1 + 100) % 100;

    if (raceCount == 0)
        raceCount = 1;

    handleBackToRoot();
    doRaceCount();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Heat # increment button.
//

void handleIncrementHeat() {
    heatCount = (heatCount + 1) % 100;   // keep 2‑digit wraparound
    raceCount = 1;

    handleBackToRoot();
    doRaceCount();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Heat # decrement button.
//

void handleDecrementHeat() {
    heatCount = (heatCount - 1 + 100) % 100;
    raceCount = 1;

    handleBackToRoot();
    doRaceCount();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Practice button.
//

void handlePractice() {
    handleBackToRoot();
    doPractice();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Reset button. This clears the stored WiFi SSID and password, shows a message on the display, and restarts the device after a delay.
//

void handleReset() {

    int16_t H;

    prefs.begin("wifi", false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();

    epaper.fillScreen(TFT_WHITE);
    H = epaper.fontHeight();
    epaper.setCursor(0, H);              // center top
    epaper.print("Old SSID and password deleted\nRestarting in 5 seconds...");
    epaper.update();
    delay(5000);

    heatCount = 0;
    raceCount = 1;
    practiceMode = false;

    ESP.restart();
}


//////////////////////////////////////////////////////////////////////////////
//
// this handles the RESET button on the main page, which resets heat and race and practice and then throws the splash screen up again.
//

void handleSplash() {
    handleBackToRoot();
    heatCount = 0;
    raceCount = 0;
    practiceMode = false;
    doWelcomeScreen();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Set Heat button. This looks for a number in the input box, and if it's valid, it sets the heat count to that number. 
// If it's not valid, it leaves it unchanged. 
// If you click Set when the heat count is currently zero ("None" displayed in the input box), it sets it to 1. 
// If you enter an out of range value, it is ignored.
//

void handleSetHeat() {      // can't enter a number in the box after startup, set to readonly, it says "None". Clicking SET advances it to 1 and sets it to changeable

    handleBackToRoot();

    int v = 1;              // if you click SET and race number has not already been set, it sets race number to 1. If you enter an out of range value, it is ignored.

    if (server.hasArg("v1")) {
        v = server.arg("v1").toInt();
        if (v < 0 || v > 99)
            v = heatCount;
        }
    
    heatCount = v;

    if(heatCount)
        raceCount = 1;

    practiceMode = false;
    doRaceCount();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Set Race button. This looks for a number in the input box, and if it's valid, it sets the race count to that number. 
// If it's not valid, it leaves it unchanged. If you click Set when the race count is currently zero ("None" displayed in the input box), it sets it to 1. 
// If you enter an out of range value, it is ignored.
//

void handleSetRace() {      // same functionality as above, but for the set race input box. Difference here, heat is allowed to be zero.
    int v = 1;

    if (server.hasArg("v2")) {
        v = server.arg("v2").toInt();
        if (v < 1 || v > 99)
            v = raceCount;
    }

    raceCount = v;

    practiceMode = false;
    doRaceCount();

    handleBackToRoot();
}


//////////////////////////////////////////////////////////////////////////////
//
// This handler is only invoked when the device is in AP mode, which happens when there is no stored WiFi SSID and password. 
// It generates the HTML for the page that allows you to enter a WiFi SSID and password, and submit them to be saved in preferences.
//

void handleRootAP() {
    String page = "<!DOCTYPE html><html><body style='font-family:sans-serif;'><head>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>";
    page += "<style>";
    page += "h1{font-size:26px;margin:8px 0 12px 0;font-weight:bold;}";
    page += "h2{font-size:20px;margin:4px 0;font-weight:bold;}";
    page += "h3{font-size:18px;margin:4px 0;font-weight:bold;}";
    page += "h4{font-size:16px;margin:3px 0;font-weight:bold;}";
    
    page += "button{font-size:20px;height:40px;margin:2px;padding:0 10px;border-radius:6px;border:1px solid #666;background:#e0e0e0;}";
    page += "button:active{background:#ccc;}";

    page += "</style>";

    page += "<h2>Race Count Display WiFi Setup</h2>";
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
// This function is called at startup if there is no stored WiFi SSID and password. 
// It sets up the device as a WiFi access point, displays the AP information on the screen, and starts the web server to handle configuration requests.
//

void startConfigAP() {
    Serial.println("Starting WiFi setup AP...");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("RaceCounter-Setup");

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    epaper.setFreeFont(&FreeSansBold12pt7b);

    epaper.print("\nSSID: RaceCounter-Setup");
    epaper.print("AP IP: ");
    epaper.println(WiFi.softAPIP());
    epaper.print("MAC: ");
    epaper.println(WiFi.softAPmacAddress());
	epaper.println("\npush both buttons to set standalone mode");
    epaper.update();

    server.on("/", handleRootAP);
    server.on("/save", HTTP_POST, handleSaveAP);
    server.begin();
}


//////////////////////////////////////////////////////////////////////////////
//
// generate a web page for the settings screen (accessed with the icon in the upper right corner of the screen), 
// Allows you to enter a WiFi SSID and password, and submit them to be saved in preferences. Update button will trigger a restart using the new SSID and password.
// Also includes a button to clear the stored SSID and password, which will cause the device to restart in AP mode so you can set up WiFi again.
//

void handleSettings() {

    server.on("/save", HTTP_POST, handleSaveAP);

    String page = "<!DOCTYPE html><html><head>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>";
    page += "<style>";
    page += ".main-container{max-width:480px;min-height:320px;margin:0 auto;text-align:center;}";
    page += "body{font-family:sans-serif;margin:0;padding:10px;}";
    page += "h1{font-size:26px;margin:8px 0 12px 0;font-weight:bold;}";
    page += "h2{font-size:20px;margin:4px 0;font-weight:bold;}";
    page += "h3{font-size:18px;margin:4px 0;font-weight:bold;}";
    page += "h4{font-size:14px;margin:3px 0;font-weight:normal;color:#FF0000;font-style:italic;}";
    page += "h5{font-size:14px;margin:3px 0;font-weight:bold;}";

    page += "button{font-size:20px;height:40px;margin:2px;padding:0 10px;border-radius:6px;border:1px solid #666;background:#e0e0e0;}";
    page += "button:active{background:#ccc;}";
    page += ".bottomButtons{display:flex;gap:40px;margin-top:10px;}";
    page += ".clearBtn{ grid-column:2; justify-self:end; }";

    page += ".buttonRow{"
        "display:flex;"
        "justify-content:center;"
        "gap:12px;"
        "margin-top:10px;"
        "}";

    /* Alignment fix */
    page += ".formRow{display:grid;grid-template-columns:110px 1fr;align-items:center;margin-bottom:14px;}";
    page += ".formRow label{font-size:18px;font-weight:600;}";
    page += ".pwRow label{font-size:18px;font-weight:600;}";
    page += ".pwRow{display:grid;grid-template-columns:110px 1fr;align-items:center;margin-bottom:14px;}";
    page += ".pwWrap{position:relative; max-width:260px;width:100%;}";

    page += ".inputBox{"
        "font-family:sans-serif;"
        "font-size:20px;"
        "width:100%;"
        "max-width:260px;"
        "padding:4px;"
        "border:1px solid #ccc;"
        "border-radius:4px;"
        "}";

    page += ".showPw{position:absolute;right:8px;top:50%;transform:translateY(-50%);cursor:pointer;font-size:20px;color:#666;}";

    page += ".bgImg{"
        "position:fixed;"
        "top:3%; left:3%;"
        "width:94%; height:94%;"
        "object-fit:contain;"
        "opacity:0.20;"      // adjust fade here
        "z-index:-1;"
        "}";

    page += ".main-container{"
        "max-width:480px;"
        "margin:0 auto;"        /* centers the whole block */
        "text-align:center;"    /* centers headings + buttons */
        "}";

    page += "</style></head><body>";
    page += "<img src='/bg.png' class='bgImg'><div class='main-container'>";

    page += "<h1>Race Count Display Settings</h1>";
    page += "<form action='/save' method='POST'>";

    ssidStored.replace("'", "&#39;");       // single quotes will break HTML if they're included in the SSID or password
    passStored.replace("'", "&#39;");

    page += "<div class='formRow'><label>SSID:</label><input class='inputBox' name='ssid' value='" + ssidStored + "'></div>";
    page += "<div class='pwRow'><label>Password:</label>";
    page += "<div class='pwWrap'>";
    page += "<input class='inputBox' name='pass' id='pw' type='password' value='" + passStored + "'>";
    page += "<span class='showPw' onclick='togglePw()'>&#128065;&#65039;</span>";  // 👁️ icon
    page += "</div></div>";

    page += "<div class='buttonRow'>";
    page += "  <button type='submit' class='updateBtn'>Update</button>";
    page += "  <button type='button' onclick=\"location.href='/reset'\">Clear</button>";
    page += "</div>";

    page += "<h4><br>Pressing either button will initiate a reboot<br></h4><h5>Use browser [BACK] to quit without saving</h5>";

    page += "</form>";
    page += "</div>";

    page += "<script>";
    page += "function togglePw(){";
    page += "  var p=document.getElementById('pw');";
    page += "  p.type = (p.type==='password') ? 'text' : 'password';";
    page += "}";
    page += "</script>";

    page += "</body></html>";

    server.send(200, "text/html", page);
}


//////////////////////////////////////////////////////////////////////////////
//
// There are a couple of places where pushing both buttons on the display unit at the same time triggers a reset of the WiFi credentials. 
// Checked immediately on startup, and during the Wifi connection loop (so the loop can be aborted if the credentials turn out to be no good).
// This function is called in those places and if both buttons are presseed, it clears the stored SSID and password, shows a message on the display, and restarts after a delay.
//

void checkClearAll(uint8_t H) {
    if ((digitalRead(UPSWITCH) == LOW) && (digitalRead(DNSWITCH) == LOW)) {
        Serial.println("Clearing SSID and Password, then restarting");

        prefs.begin("wifi", false);
        prefs.remove("ssid");
        prefs.remove("pass");
        prefs.end();

        prefs.begin("SA", false);              // standalone mode
        prefs.remove("SA");
        prefs.end();

        epaper.setFreeFont(&FreeSansBold12pt7b);

        epaper.fillScreen(TFT_WHITE);
        epaper.setCursor(0, H);              // center top
        epaper.print("Old SSID and password deleted\nStandalone disabled\nRestarting in 5 seconds");
        epaper.update();
        delay(5000);
        ESP.restart();
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// Setup 
//

void setup()
{
    int16_t     X, Y;
    uint16_t    W, H;

    char        charBuff[128];

    Serial.begin(115200);

    delay(2000);

    Serial.println("\n\n\n------Race Counter-----");

    pinMode(UPSWITCH, INPUT_PULLUP);
    pinMode(DNSWITCH, INPUT_PULLUP);
    pinMode(WAKEUPPIN, INPUT_PULLUP);

    raceCount = 0;
    practiceMode = false;
    justPoweredUp = true;

    epaper.begin();
    epaper.fillScreen(TFT_WHITE);
    epaper.setFreeFont(&FreeSansBold12pt7b);
    epaper.setTextSize(1);
    H = epaper.fontHeight();

	checkClearAll(H);                       // check if both buttons are pressed on startup, if so, does not return, clears WiFi credentials and restarts
	                                        // which then comes back through setup again, but with cleared credentials 

    prefs.begin("wifi", true);
    ssidStored = prefs.getString("ssid", "");
    passStored = prefs.getString("pass", "");
    prefs.end();

    prefs.begin("SA", true);                // standalone mode
    standAlone = prefs.getString("SA", "");
    prefs.end();

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

    sprintf(charBuff, "Connecting to SSID: %s\n", ssidStored.c_str());
    Serial.print(charBuff);

    epaper.setCursor(0, H);              // center top
    epaper.print(charBuff);
    epaper.update();

    WiFi.begin(ssidStored, passStored);

    epaper.setFreeFont(&FreeSans12pt7b);

    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
        epaper.print(".");
        epaper.update();
        checkClearAll(H);
    }

    Serial.println("\nConnected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    epaper.print("...Connected!\n");

    epaper.setFreeFont(&FreeSansBold12pt7b);
    epaper.print("\nIPAddress: ");
    epaper.print(WiFi.localIP());
    epaper.update();

    server.on("/", handleRoot);
    server.on("/heatInc", handleIncrementHeat);
    server.on("/heatDec", handleDecrementHeat);
    server.on("/heatSet", handleSetHeat);
    server.on("/raceInc", handleIncrementRace);
    server.on("/raceDec", handleDecrementRace);
    server.on("/raceSet", handleSetRace);
    server.on("/practice", handlePractice);
    server.on("/settings", handleSettings);
    server.on("/reset", handleReset);
    server.on("/splash", handleSplash);

    SPIFFS.begin(true);

    /* uncomment to list files in SPIFFS, for debugging
    Serial.println("SPIFFS contents:");

    File root = SPIFFS.open("/");
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }
   
    File file = root.openNextFile();
    while (file) {
        Serial.print("FILE: ");
        Serial.print(file.name());
        Serial.print("  SIZE: ");
        Serial.println(file.size());
        file = root.openNextFile();
    }
    */

    server.on("/bg.png", []() {
        File file = SPIFFS.open("/bg.png", "r");
        if (!file) {
            server.send(404, "text/plain", "File not found");
            Serial.println("SPIFFS open failed for bg.png");
            return;
        }

        server.streamFile(file, "image/png");
        file.close();
        });

    server.begin();

    delay(2000);

    doWelcomeScreen();
}


//////////////////////////////////////////////////////////////////////////////
//
// Main loop
//

void loop()
{
    unsigned long startMillis;

    server.handleClient();

    if (!digitalRead(UPSWITCH) || !digitalRead(DNSWITCH)) {     // if either switch is pressed, wait a bit to see if the other gets pressed - active low. Can be hard to hit both buttons at once
        startMillis = millis();

        while(millis() < startMillis+500)
            if (!digitalRead(UPSWITCH) && !digitalRead(DNSWITCH)) {
                if (!inAPMode)
                    doPractice();
                else
                    setStandAlone();                             // if we're in AP mode and both buttons are pressed, turn on standalone mode - this will force a restart
                return;
            }
    }

    if (inAPMode)
		return; 											 // if we're in AP mode, we don't want the buttons to do anything except trigger standalone mode, so skip the rest of the loop

    if (digitalRead(UPSWITCH) == LOW) {
        Serial.println("Up Switch pressed!");

        if (++raceCount == 100)
            raceCount = 1;
        doRaceCount();
        practiceMode = false;
        return;
    }

    if ((digitalRead(DNSWITCH) == LOW) && (raceCount != 0)) {
        Serial.println("Down Switch pressed!");

        --raceCount;
        if (raceCount > 99)
            raceCount = 99;

        if (raceCount == 0)
            raceCount = 99;

        doRaceCount();
        practiceMode = false;
        return;
    }

}
