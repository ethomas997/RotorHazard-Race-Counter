//////////////////////////////////////////////////////////////////////////////
//
// Web module: the web interface and HTTP API served while connected to WiFi - main page, settings page and
// the handlers behind their buttons. The AP-mode setup page lives in wifi_config.cpp.
//
//////////////////////////////////////////////////////////////////////////////

#include "RaceCounter.h"
#include "display.h"
#include "web.h"
#include "wifi_config.h"

#include "bg_png.h"

WebServer server(80);


//////////////////////////////////////////////////////////////////////////////
//
// Generate the HTML for the main page, which includes the current heat and race count and buttons to increment/decrement/set them, as well as the Practice and Reset buttons.
//

static String handleMainPage() {
    String page = "<!DOCTYPE html><html><head><title>" PAGE_TITLE "</title><meta name='viewport' content='width=device-width, initial-scale=1'>";

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

    page += "<h1>" PAGE_TITLE "</h1>";

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

static void handleRoot() {
    server.send(200, "text/html", handleMainPage());
}


//////////////////////////////////////////////////////////////////////////////
//
// ESP32 leaves the string for the item that was clicked in the browser address bar, so refresh makes it do the same thing again
// Calling this routine at the end of each response throws the browser back to the root page again
//

static void handleBackToRoot() {
    server.sendHeader("Location", "/");
    server.send(303);
}


//////////////////////////////////////////////////////////////////////////////
//
// the Race # increment button
//

static void handleIncrementRace() {
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

static void handleDecrementRace() {
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

static void handleIncrementHeat() {
    heatCount = (heatCount + 1) % 100;   // keep 2‑digit wraparound
    raceCount = 1;
    bannerText = "";                    // a manual heat change replaces any banner override

    handleBackToRoot();
    doRaceCount();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Heat # decrement button.
//

static void handleDecrementHeat() {
    heatCount = (heatCount - 1 + 100) % 100;
    raceCount = 1;
    bannerText = "";

    handleBackToRoot();
    doRaceCount();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Practice button.
//

static void handlePractice() {
    handleBackToRoot();
    doPractice();
}

//////////////////////////////////////////////////////////////////////////////
//
// this handles the RESET button on the main page, which resets heat and race and practice and then throws the splash screen up again.
//

static void handleSplash() {
    handleBackToRoot();
    heatCount = 0;
    raceCount = 0;
    practiceMode = false;
    bannerText = "";
    updateDisplay();
}


//////////////////////////////////////////////////////////////////////////////
//
// the Set Heat button. This looks for a number in the input box, and if it's valid, it sets the heat count to that number. 
// If it's not valid, it leaves it unchanged. 
// If you click Set when the heat count is currently zero ("None" displayed in the input box), it sets it to 1. 
// If you enter an out of range value, it is ignored.
//

static void handleSetHeat() {      // can't enter a number in the box after startup, set to readonly, it says "None". Clicking SET advances it to 1 and sets it to changeable

    handleBackToRoot();

    int v = 1;              // if you click SET and race number has not already been set, it sets race number to 1. If you enter an out of range value, it is ignored.

    if (server.hasArg("v1")) {
        v = server.arg("v1").toInt();
        if (v < 0 || v > 99)
            v = heatCount;
        }
    
    heatCount = v;
    bannerText = "";

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

static void handleSetRace() {      // same functionality as above, but for the set race input box. Difference here, heat is allowed to be zero.
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
// generate a web page for the settings screen (accessed with the icon in the upper right corner of the screen), 
// Allows you to enter a WiFi SSID and password, and submit them to be saved in preferences. Update button will trigger a restart using the new SSID and password.
// Also includes a button to clear the stored SSID and password, which will cause the device to restart in AP mode so you can set up WiFi again.
//

static void handleSettings() {

    String page = "<!DOCTYPE html><html><head><title>" PAGE_TITLE " Settings</title>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>";
    page += "<style>";
    page += ".main-container{max-width:480px;min-height:320px;margin:0 auto;text-align:center;}";
    page += "body{font-family:sans-serif;margin:0;padding:10px;}";
    page += "h1{font-size:26px;margin:8px 0 12px 0;font-weight:bold;}";
    page += "h2{font-size:20px;margin:4px 0;font-weight:bold;}";
    page += "h3{font-size:18px;margin:4px 0;font-weight:bold;}";
    page += "h4{font-size:14px;margin:3px 0;font-weight:normal;color:#FF0000;font-style:italic;}";
    page += "h5{font-size:14px;margin:3px 0;font-weight:bold;}";
    page += ".version{font-size:12px;color:#888;margin-top:24px;}";

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

    page += "<h1>" PAGE_TITLE " Settings</h1>";
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
    page += "  <button type='submit' form='clearForm'>Clear</button>";       // submits the separate POST form below, not this one
    page += "</div>";

    page += "<h4><br>Pressing either button will initiate a reboot<br></h4><h5>Use browser [BACK] to quit without saving</h5>";

    page += "</form>";

    page += "<p class='version'>Firmware v" FW_VERSION "</p>";

    // Clearing the credentials is a POST with a confirmation so it can't be triggered by a stray link, prefetch or browser history entry
    page += "<form id='clearForm' method='POST' action='/reset' onsubmit=\"return confirm('Erase the saved WiFi SSID and password and reboot into setup mode?')\"></form>";

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
// Registers the web server routes and starts the server. Called once WiFi is connected.
//
//////////////////////////////////////////////////////////////////////////////

void startWebServer() {
    server.on("/", handleRoot);
    server.on("/heatInc", handleIncrementHeat);
    server.on("/heatDec", handleDecrementHeat);
    server.on("/heatSet", handleSetHeat);
    server.on("/raceInc", handleIncrementRace);
    server.on("/raceDec", handleDecrementRace);
    server.on("/raceSet", handleSetRace);
    server.on("/practice", handlePractice);
    server.on("/settings", handleSettings);
    server.on("/save", HTTP_POST, handleSaveAP);    // settings page "Update" button
    server.on("/reset", HTTP_POST, handleReset);   // POST only: erases the WiFi credentials, so a plain GET must not be able to trigger it
    server.on("/splash", handleSplash);

    server.on("/bg.png", []() {                    // web page background image, embedded in the firmware (see bg_png.h)
        server.sendHeader("Cache-Control", "max-age=86400");
        server.send_P(200, "image/png", (PGM_P)BG_PNG, BG_PNG_LEN);
        });

    server.begin();
}
