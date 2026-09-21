//////////////////////////////////////////////////////////////////////////////
//
// RotorHazard client module - see rh_client.h.
//
// How it works:
//   * A Socket.IO (Engine.IO v4) connection to the server. On connect we join the default namespace and
//     ask for the current state with a load_data event, exactly as the RotorHazard web pages do. Neither
//     needs the admin password; only the events that change things on the server are protected.
//   * The server broadcasts race_status and current_heat whenever a race is staged, started, stopped,
//     saved or the heat is changed. Both carry the heat id and next_round - the round the next saved race
//     will get, i.e. the one that is being run or about to be run. That is what the panel shows - except
//     that once a race has been stopped (race_status DONE, not yet saved) the panel already shows the round
//     after it, so people see what's coming next without waiting for the save.
//   * The heat's display name isn't in those events, so it's fetched once per heat from GET /api/heat/<id>.
//   * Everything else the server broadcasts (heartbeat, leaderboards, results...) is ignored. Frames bigger
//     than the WebSockets library's 15 KB limit (the results after each race save) make it drop the
//     connection; it reconnects by itself and we re-request the state, so nothing is lost.
//
//////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <HTTPClient.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>

#include "RaceCounter.h"
#include "display.h"
#include "rh_client.h"

#define RH_DEFAULT_PORT         5000
#define RH_RECONNECT_MS         5000    // how long the Socket.IO client waits before reconnecting after a drop
#define RH_SETTLE_MS            250     // events arrive in bursts (race_status + current_heat); wait this long after the last one before redrawing
#define RH_HTTP_TIMEOUT_MS      3000

// RotorHazard race_status values (RHRace.py, class RaceStatus)
#define RH_RACE_READY           0
#define RH_RACE_RACING          1
#define RH_RACE_DONE            2
#define RH_RACE_STAGING         3

String rhServer = "";

static SocketIOclient   sio;
static String           host;
static uint16_t         port = RH_DEFAULT_PORT;
static bool             enabled = false;
static bool             connected = false;

static int              heatId = -1;            // last state received from the server
static int              roundNum = -1;
static int              raceStatus = RH_RACE_READY;    // from the last race_status event
static bool             pending = false;        // state received, not yet applied to the display
static unsigned long    lastEventAt = 0;

static int              formatId = -1;          // race format in effect (from race_status)

static int              nameHeatId = -1;        // the heat whose name is cached
static String           heatName;
static int              nameFormatId = -1;      // the race format whose name is cached
static String           formatName;
static bool             formatIsPractice = false;   // the cached format's name contains "practice"


//////////////////////////////////////////////////////////////////////////////
//
// GETs a JSON API endpoint and parses it through a filter (the responses can be large - a heat's answer
// includes its whole leaderboard - so only the wanted fields are kept).
//

static bool getJson(const String &path, JsonDocument &filter, JsonDocument &doc) {
    WiFiClient  client;
    HTTPClient  http;
    String      url = "http://" + host + ":" + String(port) + path;

    http.setTimeout(RH_HTTP_TIMEOUT_MS);
    if (!http.begin(client, url))
        return false;

    int code = http.GET();
    if (code < 200 || code >= 300) {            // (RotorHazard answers these with 201, oddly)
        Serial.printf("[RH] GET %s -> %d\n", url.c_str(), code);
        http.end();
        return false;
    }

    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        Serial.printf("[RH] GET %s: bad JSON (%s)\n", url.c_str(), err.c_str());
        return false;
    }
    return true;
}

// The display name of a heat: GET /api/heat/<id> -> {"heat":{"setup":{"displayname":...},...}}

static bool fetchHeatName(int id, String &name) {
    JsonDocument filter, doc;
    filter["heat"]["setup"]["displayname"] = true;

    if (!getJson("/api/heat/" + String(id), filter, doc))
        return false;

    const char *n = doc["heat"]["setup"]["displayname"];
    if (!n || !*n)
        return false;
    name = n;
    return true;
}

// The name of a race format: GET /api/format/<id> -> {"format":{"name":...,...}}

static bool fetchFormatName(int id, String &name) {
    JsonDocument filter, doc;
    filter["format"]["name"] = true;

    if (!getJson("/api/format/" + String(id), filter, doc))
        return false;

    const char *n = doc["format"]["name"];
    if (!n || !*n)
        return false;
    name = n;
    return true;
}

// True if the text contains "practice" in any letter case.

static bool mentionsPractice(const String &text) {
    String lower = text;
    lower.toLowerCase();
    return lower.indexOf("practice") >= 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// Puts the last received state on the panel. Two kinds of practice on the timer:
//   * heat id 0 - "Practice Mode" selected instead of a heat: the PRACTICE screen with the big "P";
//   * a real heat run with a practice race format (any format whose name contains "practice", e.g. the
//     stock "Open Practice"): "PRACTICE" as the banner, but the round number below it as usual.
//

static void applyState() {
    if (heatId == 0) {
        practiceMode = true;
        bannerText = "";
    }
    else {
        if (heatId != nameHeatId) {             // new heat: look up its name (once)
            if (!fetchHeatName(heatId, heatName))
                heatName = "Heat " + String(heatId);
            nameHeatId = heatId;
        }
        if (formatId != nameFormatId) {         // new race format: look up its name (once)
            if (!fetchFormatName(formatId, formatName))
                formatName = "";
            formatIsPractice = mentionsPractice(formatName);
            nameFormatId = formatId;
        }
        practiceMode = false;
        bannerText   = formatIsPractice ? "PRACTICE" : heatName;
        heatCount    = heatId > 99 ? 99 : heatId;
        int shown = roundNum;
        if (shown >= 1 && raceStatus == RH_RACE_DONE)  // race stopped but not saved yet: show the round that comes next
            shown++;
        if (shown >= 1)
            raceCount = shown > 99 ? 99 : shown;
        else if (raceCount == 0)
            raceCount = 1;
    }

    Serial.printf("[RH] showing %s\n", practiceMode ? "practice" : (bannerText + " / race " + String(raceCount)).c_str());
    updateDisplay();
}


//////////////////////////////////////////////////////////////////////////////
//
// A Socket.IO event has arrived: the payload is a JSON array, [name, data]. Only race_status and
// current_heat are of interest, and only three fields of those, so a filter keeps the parse small.
//

static void handleEvent(uint8_t *payload, size_t length) {
    JsonDocument filter;
    filter[0] = true;
    filter[1]["race_heat_id"] = true;
    filter[1]["current_heat"] = true;
    filter[1]["next_round"]   = true;
    filter[1]["race_status"]  = true;
    filter[1]["race_format_id"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter)))
        return;

    const char *name = doc[0];
    if (!name)
        return;

    // The heat id is null (newer servers) or 0 (older ones) when the timer is in Practice Mode rather than on
    // a heat; either way that's heat 0 here.
    JsonVariantConst heat;
    if (!strcmp(name, "race_status")) {
        heat       = doc[1]["race_heat_id"];
        raceStatus = doc[1]["race_status"] | RH_RACE_READY;
        formatId   = doc[1]["race_format_id"] | -1;
    }
    else if (!strcmp(name, "current_heat")) {
        heat       = doc[1]["current_heat"];
        raceStatus = RH_RACE_READY;             // a heat change means the previous race was saved or discarded
    }
    else
        return;                                 // heartbeat, leaderboard, ... - not ours

    heatId      = heat.isNull() ? 0 : heat.as<int>();
    roundNum    = doc[1]["next_round"] | -1;    // null while in practice mode
    pending     = true;
    lastEventAt = millis();

    Serial.printf("[RH] %s: heat %d, next round %d, race status %d\n", name, heatId, roundNum, raceStatus);
}


//////////////////////////////////////////////////////////////////////////////
//
// Socket.IO client callback.
//

static void onSocketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case sIOtype_CONNECT:                   // the WebSocket is up
            Serial.println("[RH] connected");
            sio.send(sIOtype_CONNECT, "/");     // join the default namespace (Socket.IO v3+ doesn't do this automatically)
            sio.sendEVENT("[\"load_data\",{\"load_types\":[\"race_status\",\"current_heat\"]}]");
            connected = true;
            break;

        case sIOtype_DISCONNECT:
            if (connected)
                Serial.println("[RH] disconnected");
            connected = false;
            break;

        case sIOtype_EVENT:
            handleEvent(payload, length);
            break;

        default:
            break;
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// Public interface.
//

String rhNormalizeServer(String s) {
    s.trim();
    if (s.startsWith("http://"))
        s = s.substring(7);
    else if (s.startsWith("https://"))
        s = s.substring(8);
    while (s.endsWith("/"))
        s.remove(s.length() - 1);
    return s;
}

void rhBegin() {
    enabled = false;
    rhServer = rhNormalizeServer(rhServer);     // (in case the stored value predates the normalising)
    if (rhServer.length() == 0)
        return;

    int colon = rhServer.indexOf(':');
    if (colon > 0) {
        host = rhServer.substring(0, colon);
        port = rhServer.substring(colon + 1).toInt();
        if (port == 0)
            port = RH_DEFAULT_PORT;
    }
    else {
        host = rhServer;
        port = RH_DEFAULT_PORT;
    }

    Serial.printf("[RH] following RotorHazard server %s:%u\n", host.c_str(), port);

    sio.onEvent(onSocketIOEvent);
    sio.setReconnectInterval(RH_RECONNECT_MS);
    sio.begin(host, port, "/socket.io/?EIO=4");
    enabled = true;
}

void rhLoop() {
    if (!enabled)
        return;

    sio.loop();

    if (pending && millis() - lastEventAt >= RH_SETTLE_MS) {
        pending = false;
        applyState();
    }
}

bool rhConfigured() { return rhServer.length() > 0; }
bool rhConnected()  { return enabled && connected; }
int  rhHeatId()     { return heatId; }
int  rhRound()      { return roundNum; }

String rhHeatName() {
    return nameHeatId >= 0 ? heatName : String("");
}

String rhStatusText() {
    if (!rhConfigured())
        return "not configured";
    if (!enabled)
        return "waiting for WiFi";
    return connected ? "connected" : "connecting";
}
