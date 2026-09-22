//////////////////////////////////////////////////////////////////////////////
//
// Web module: the web interface and HTTP API served while connected to WiFi - main page, settings page and
// the handlers behind their buttons. The AP-mode setup page lives in wifi_config.cpp.
//
// The pages are static HTML/CSS held in flash as raw string literals, with the few dynamic values (the heat
// and race inputs, the practice button state, the saved SSID / password) sent in between as separate chunks
// (HTTP chunked transfer). Nothing is assembled into a String, so a page request doesn't churn the heap.
//
//////////////////////////////////////////////////////////////////////////////

#include "RaceCounter.h"
#include "display.h"
#include "web.h"
#include "wifi_config.h"
#include "rh_client.h"

#include <WiFi.h>

#include "bg_png.h"

WebServer server(80);


//////////////////////////////////////////////////////////////////////////////
//
// Page templates. Each page is split where a dynamic value goes.
//

static const char MAIN_PAGE_1[] PROGMEM = R"html(<!DOCTYPE html><html><head><title>)html" PAGE_TITLE R"html(</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body{font-family:sans-serif;margin:0;padding:0;background:#fafafa;}
.main-container{max-width:480px;min-height:320px;margin:0 auto;padding:10px;}
h1{font-size:26px;margin:8px 0 12px 0;font-weight:bold;text-align:center;}
h2{font-size:20px;margin:4px 0;font-weight:bold;}
h3{font-size:18px;margin:4px 0;font-weight:bold;}
h4{font-size:16px;margin:3px 0;font-weight:bold;}
table{border-collapse:collapse;width:100%;}
td{padding:4px;height:50px;vertical-align:middle;}
col.inputcol{width:60px;}
col.setcol{width:100px;}
col.smallcol{width:50px;}
col.textcol{width:40px;}
input{font-size:24px;width:60px;height:34px;text-align:center;margin:2px;padding:2px;border:2px solid #666;border-radius:4px;}
button{font-size:20px;height:40px;margin:2px;padding:0 10px;border-radius:6px;border:2px solid #666;background:#e0e0e0;}
button:active{background:#ccc;}
.incdec{width:40px;height:40px;font-size:24px;border-radius:6px;border:2px solid #666;display:flex;justify-content:center;align-items:center;background:#ddd;}
.practiceOn{width:120px;background:#00ff00;color:black;border:2px solid #666;}
.practiceOff{width:120px;color:green;font-weight:600;background:#f0f0f0;border:2px solid #0a0;}
.resetBut{width:120px;color:red;font-weight:600;background:#f0f0f0;border:2px solid #a00;}
.bottomButtons{display:flex;gap:20px;justify-content:center;margin-top:10px;}
.settingsIcon{position:fixed;top:10px;right:10px;font-size:32px;cursor:pointer;text-decoration:none;color:#333;}
.showing{text-align:center;font-size:18px;margin:0 0 10px 0;}
.rhlink{text-align:center;font-size:15px;margin:16px 0 0 0;color:#444;}
.active{color:#080;font-weight:bold;}
.lost{color:#c00;font-weight:bold;}
.version{text-align:center;font-size:12px;color:#888;margin-top:24px;}
.refreshBut{width:120px;font-weight:600;background:#f0f0f0;border:2px solid #666;}
.bgImg{position:fixed;top:3%;left:3%;width:94%;height:94%;object-fit:contain;opacity:0.20;z-index:-1;}
</style></head>
<body><img src='/bg.png' class='bgImg'><div class='main-container'>
<a href='/settings' class='settingsIcon'>&#128736;</a>
<h1>)html" PAGE_TITLE R"html(</h1>
<p class='showing'>Showing: <b>)html";
//  ... what the panel shows ...
static const char MAIN_PAGE_1B[] PROGMEM = R"html(</b></p>
<table><colgroup><col class='textcol'><col class='smallcol'><col class='smallcol'><col class='inputcol'><col class='setcol'></colgroup>
<tr><td><h2>Heat </h2></td>
<td><button class='incdec' onclick="location.href='/heatDec'">-</button></td>
<td><button class='incdec' onclick="location.href='/heatInc'">+</button></td>
<td><form action='/heatSet' method='GET'>
)html";
//  ... the heat input ...
static const char MAIN_PAGE_2[] PROGMEM = R"html(
</td><td><button type='submit'>Set</button></form></td></tr>
<tr><td><h2>Race </h2></td>
<td><button class='incdec' onclick="location.href='/raceDec'">-</button></td>
<td><button class='incdec' onclick="location.href='/raceInc'">+</button></td>
<td><form action='/raceSet' method='GET'>
)html";
//  ... the race input ...
static const char MAIN_PAGE_3[] PROGMEM = R"html(
</td><td><button type='submit'>Set</button></form></td></tr>
</table><br>
<div class='bottomButtons'>
<button class=')html";
//  ... practiceOn / practiceOff ...
static const char MAIN_PAGE_4[] PROGMEM = R"html(' onclick="location.href='/practice'">Practice</button>
<button class='resetBut' onclick="location.href='/splash'">Reset</button>
<button class='refreshBut' onclick="location.href='/'">Refresh</button>
</div><br>
<p class='rhlink'>)html";
//  ... the RotorHazard connection line ...
static const char MAIN_PAGE_5[] PROGMEM = R"html(</p>
<p class='version'>Firmware v)html" FW_VERSION R"html(</p>
</div></body></html>
)html";

static const char SETTINGS_PAGE_1[] PROGMEM = R"html(<!DOCTYPE html><html><head><title>)html" PAGE_TITLE R"html( Settings</title>
<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>
<style>
body{font-family:sans-serif;margin:0;padding:10px;}
h1{font-size:26px;margin:8px 0 12px 0;font-weight:bold;}
h2{font-size:20px;margin:4px 0;font-weight:bold;}
h3{font-size:18px;margin:4px 0;font-weight:bold;}
h4{font-size:14px;margin:3px 0;font-weight:normal;color:#FF0000;font-style:italic;}
h5{font-size:14px;margin:3px 0;font-weight:bold;}
.mac{font-size:18px;margin-top:24px;}
.mac b{font-family:monospace;font-size:20px;letter-spacing:1px;}
.version{font-size:12px;color:#888;margin-top:6px;}
button{font-size:20px;height:40px;margin:2px;padding:0 10px;border-radius:6px;border:1px solid #666;background:#e0e0e0;}
button:active{background:#ccc;}
.buttonRow{display:flex;justify-content:center;gap:12px;margin-top:10px;}
.formRow{display:grid;grid-template-columns:135px 1fr;align-items:center;margin-bottom:14px;}
.formRow label{font-size:18px;font-weight:600;}
.pwRow{display:grid;grid-template-columns:135px 1fr;align-items:center;margin-bottom:14px;}
.pwRow label{font-size:18px;font-weight:600;}
.pwWrap{position:relative;max-width:260px;width:100%;}
.inputBox{font-family:sans-serif;font-size:20px;width:100%;max-width:260px;padding:4px;border:1px solid #ccc;border-radius:4px;}
.showPw{position:absolute;right:8px;top:50%;transform:translateY(-50%);cursor:pointer;font-size:20px;color:#666;}
.bgImg{position:fixed;top:3%;left:3%;width:94%;height:94%;object-fit:contain;opacity:0.20;z-index:-1;}
.main-container{max-width:480px;margin:0 auto;text-align:center;}
</style></head><body>
<img src='/bg.png' class='bgImg'><div class='main-container'>
<h1>)html" PAGE_TITLE R"html( Settings</h1>
<form action='/save' method='POST'>
<div class='formRow'><label>SSID:</label><input class='inputBox' name='ssid' value=')html";
//  ... the saved SSID ...
static const char SETTINGS_PAGE_2[] PROGMEM = R"html('></div>
<div class='pwRow'><label>Password:</label>
<div class='pwWrap'>
<input class='inputBox' name='pass' id='pw' type='password' value=')html";
//  ... the saved password ...
static const char SETTINGS_PAGE_3[] PROGMEM = R"html('>
<span class='showPw' onclick='togglePw()'>&#128065;&#65039;</span>
</div></div>
<div class='formRow'><label>RotorHazard:&nbsp;</label><input class='inputBox' name='rh' placeholder='host or host:port' value=')html";
//  ... the RotorHazard server ...
static const char SETTINGS_PAGE_4[] PROGMEM = R"html('></div>
<h5>RotorHazard server to follow (blank = none)</h5>
<div class='buttonRow'>
  <button type='submit' class='updateBtn'>Update</button>
  <button type='submit' form='clearForm'>Clear</button>
</div>
<h4><br>Pressing either button will initiate a reboot<br></h4><h5>Use browser [BACK] to quit without saving</h5>
</form>
<p class='mac'>MAC address: <b>)html";
//  ... the station MAC address ...
static const char SETTINGS_PAGE_5[] PROGMEM = R"html(</b></p>
<p class='version'>Firmware v)html" FW_VERSION R"html(</p>
<form id='clearForm' method='POST' action='/reset' onsubmit="return confirm('Erase the saved WiFi SSID and password and reboot into setup mode?')"></form>
</div>
<script>
function togglePw(){
  var p=document.getElementById('pw');
  p.type = (p.type==='password') ? 'text' : 'password';
}
</script>
</body></html>
)html";


//////////////////////////////////////////////////////////////////////////////
//
// Helpers for sending a page: a chunked response whose static parts come straight from flash.
//

static void beginPage() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);    // chunked transfer
    server.send(200, "text/html", "");
}

static void endPage() {
    server.sendContent("");                             // the terminating chunk
}

// Sends a dynamic value. An empty chunk would end the response (see endPage), so empty values are skipped.

static void sendValue(const String &v) {
    if (v.length())
        server.sendContent(v);
}

// Makes a value safe to put inside a single-quoted HTML attribute or in element text.

static String htmlEscape(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '\'': out += "&#39;";  break;
            case '"':  out += "&quot;"; break;
            default:   out += c;        break;
        }
    }
    return out;
}


//////////////////////////////////////////////////////////////////////////////
//
// The main page: the current heat and race count, buttons to increment/decrement/set them, and the Practice and Reset buttons.
//

static void handleRoot() {
    beginPage();
    server.sendContent_P(MAIN_PAGE_1);
    server.sendContent(htmlEscape(displayDescription()));
    server.sendContent_P(MAIN_PAGE_1B);

    if (heatCount)                                      // heat 0 shows as "None" and can't be typed into: use + or Set to start it
        server.sendContent("<input type='number' name='v1' min='0' max='99' value='" + String(heatCount) + "'>");
    else
        server.sendContent("<input type='text' value='None' readonly>");

    server.sendContent_P(MAIN_PAGE_2);

    if (raceCount)
        server.sendContent("<input type='number' name='v2' min='1' max='99' value='" + String(raceCount) + "'>");
    else
        server.sendContent("<input type='text' value='None' readonly>");

    server.sendContent_P(MAIN_PAGE_3);
    server.sendContent(practiceMode ? "practiceOn" : "practiceOff");
    server.sendContent_P(MAIN_PAGE_4);
    if (rhConfigured())
        server.sendContent("Connection to RotorHazard server at <b>" + htmlEscape(rhServerAddress()) + "</b>:&nbsp; " +
                           (rhLinkLost() ? "<span class='lost'>Lost</span>" : "<span class='active'>Active</span>"));
    else
        server.sendContent("RotorHazard server: not configured");
    server.sendContent_P(MAIN_PAGE_5);
    endPage();
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
// The settings page (accessed with the icon in the upper right corner of the main page).
// Allows you to enter a WiFi SSID and password, and submit them to be saved in preferences. Update button will trigger a restart using the new SSID and password.
// Also includes a button to clear the stored SSID and password, which will cause the device to restart in AP mode so you can set up WiFi again.
//

static void handleSettings() {
    beginPage();
    server.sendContent_P(SETTINGS_PAGE_1);
    sendValue(htmlEscape(ssidStored));                  // (escaped: a quote in the SSID or password would otherwise break the attribute)
    server.sendContent_P(SETTINGS_PAGE_2);
    sendValue(htmlEscape(passStored));
    server.sendContent_P(SETTINGS_PAGE_3);
    sendValue(htmlEscape(rhServer));
    server.sendContent_P(SETTINGS_PAGE_4);
    server.sendContent(stationMacAddress());
    server.sendContent_P(SETTINGS_PAGE_5);
    endPage();
}


//////////////////////////////////////////////////////////////////////////////
//
// GET /status - the device state as JSON, for scripts and for checking what the panel is showing without
// looking at it. Read-only. Also served in AP mode.
//

static String jsonString(const String &in) {
    String out = "\"";
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c < 0x20)         { out += ' '; }
        else                       { out += c; }
    }
    return out + "\"";
}

void handleStatus() {
    String json;
    json.reserve(512);

    json += "{\"name\":" + jsonString(PAGE_TITLE) + ",\"version\":\"" FW_VERSION "\"";

    if (standAloneMode)
        json += ",\"mode\":\"standalone\",\"ssid\":null,\"ip\":null,\"rssi\":null";
    else if (inAPMode)
        json += ",\"mode\":\"ap\",\"ssid\":\"RaceCounter-Setup\",\"ip\":" + jsonString(WiFi.softAPIP().toString()) + ",\"rssi\":null";
    else if (WiFi.status() == WL_CONNECTED)
        json += ",\"mode\":\"wifi\",\"ssid\":" + jsonString(ssidStored) + ",\"ip\":" + jsonString(WiFi.localIP().toString()) + ",\"rssi\":" + String(WiFi.RSSI());
    else
        json += ",\"mode\":\"wifi\",\"ssid\":" + jsonString(ssidStored) + ",\"ip\":null,\"rssi\":null";

    json += ",\"mac\":" + jsonString(stationMacAddress());
    json += ",\"heat\":" + String(heatCount) + ",\"race\":" + String(raceCount) + ",\"practice\":" + (practiceMode ? "true" : "false");
    json += ",\"banner\":" + jsonString(displayBanner()) + ",\"screen\":\"" + displayScreenName() + "\"";
    json += ",\"rh\":{\"server\":" + jsonString(rhServer) + ",\"state\":" + jsonString(rhStatusText())
          + ",\"heat_id\":" + String(rhHeatId()) + ",\"round\":" + String(rhRound()) + ",\"heat_name\":" + jsonString(rhHeatName())
          + ",\"names_from\":\"" + (rhUsingApi() ? "api" : "socket") + "\"}";
    json += ",\"uptime_s\":" + String(millis() / 1000) + ",\"free_heap\":" + String(ESP.getFreeHeap());
    json += "}\n";

    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}


//////////////////////////////////////////////////////////////////////////////
//
// Registers the web server routes and starts the server. Called once WiFi is connected.
//

void startWebServer() {
    server.on("/status", handleStatus);
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
