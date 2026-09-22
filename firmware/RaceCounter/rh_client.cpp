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
//   * The heat's display name and the race format's name aren't in those events. Socket first: on connect we
//     also ask for heat_list and format_data (every heat / format with its name) and keep the names in a
//     table; a rename arrives as a heat_data broadcast, which updates the table. Fallback: any name that
//     isn't in the table is fetched from GET /api/heat/<id> or /api/format/<id>.
//   * Frames bigger than the WebSockets library's 15 KB limit make it drop the connection (it reconnects by
//     itself and we re-request the state, so nothing is lost). The results broadcast after every race save
//     always does this. If the lists themselves are too big - a disconnect right after asking for them,
//     before they arrive - the socket route for names can't work on this event, so names come from /api
//     for the rest of the session (and the lists are no longer requested). The library never tells us the
//     close code, hence the inference.
//   * Everything else the server broadcasts (heartbeat, leaderboards, results...) is ignored.
//
//////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <HTTPClient.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>
#include <map>

#include "RaceCounter.h"
#include "display.h"
#include "rh_client.h"

#define RH_DEFAULT_PORT         5000
#define RH_RECONNECT_MS         2000    // how long the Socket.IO client waits before reconnecting after a drop
#define RH_LINK_TIMEOUT_MS      10000   // no traffic from the server for this long = the link is lost. The server sends a heartbeat
                                        // event every 0.5 s, so this is generous - it has to ride out the drop-and-reconnect the
                                        // oversized results broadcast causes after every race save without flagging it
#define RH_SETTLE_MS            250     // events arrive in bursts (race_status + current_heat); wait this long after the last one before redrawing
#define RH_HTTP_TIMEOUT_MS      3000
#define RH_LISTS_WAIT_MS        3000    // after connecting, how long to give heat_list / format_data before falling back to /api
#define RH_LISTS_DROP_MS        15000   // a disconnect this soon after asking for the lists, without getting them, means they were too big

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
static unsigned long    lastRxAt = 0;           // when anything at all last arrived from the server (heartbeats included)
static bool             linkLost = false;

static int              formatId = -1;          // race format in effect (from race_status)

static int              nameHeatId = -1;        // the heat whose name is cached
static String           heatName;
static int              nameFormatId = -1;      // the race format whose name is cached
static String           formatName;
static bool             formatIsPractice = false;   // the cached format's name contains "practice"

static std::map<int, String> heatNames;         // from the heat_list / heat_data events
static std::map<int, String> formatNames;       // from the format_data event
static bool             useApi = false;         // names come from /api for the rest of the session (see above)
static bool             gotHeatList = false;    // received since the last connect
static bool             gotFormatData = false;
static unsigned long    listsRequestedAt = 0;   // when the lists were last asked for (0 = not outstanding)


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
        // The heat's name: from the table the socket filled, otherwise from /api. In API mode it's looked up
        // on every update (the server sends race_status when a heat is renamed, and that's the only way to
        // notice there); in socket mode a rename arrives as heat_data and updates the table.
        String name;
        auto h = heatNames.find(heatId);
        if (!useApi && h != heatNames.end())
            heatName = h->second;
        else if ((useApi || heatId != nameHeatId) && fetchHeatName(heatId, name))
            heatName = name;
        else if (heatId != nameHeatId)          // nothing anywhere for this heat
            heatName = "Heat " + String(heatId);
        nameHeatId = heatId;

        auto f = formatNames.find(formatId);
        if (!useApi && f != formatNames.end()) {
            formatName = f->second;
            formatIsPractice = mentionsPractice(formatName);
            nameFormatId = formatId;
        }
        else if (formatId != nameFormatId) {    // new race format: look up its name (once)
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

// Reads a list of {id, displayname|name} objects into a name table.

static void readNames(JsonArrayConst list, const char *nameKey, std::map<int, String> &table) {
    for (JsonObjectConst item : list) {
        int id = item["id"] | -1;
        const char *name = item[nameKey];
        if (id >= 0 && name && *name)
            table[id] = name;
    }
}

static void handleEvent(uint8_t *rawPayload, size_t length) {
    // The buffer is parsed twice (name first, then the fields of interest), so it must be read as const:
    // ArduinoJson parses a non-const buffer in zero-copy mode, which modifies it in place.
    const char *payload = (const char *)rawPayload;

    // First just the event name (cheap: the filter drops everything else)
    JsonDocument nameFilter, nameDoc;
    nameFilter[0] = true;
    if (deserializeJson(nameDoc, payload, length, DeserializationOption::Filter(nameFilter)))
        return;
    const char *name = nameDoc[0];
    if (!name)
        return;

    // ArduinoJson applies the first element of a filter array to every element of the input array, so the
    // field filters go in filter[0] even though the fields are in payload element 1.
    JsonDocument filter, doc;

    if (!strcmp(name, "race_status") || !strcmp(name, "current_heat")) {
        filter[0]["race_heat_id"]   = true;
        filter[0]["current_heat"]   = true;
        filter[0]["next_round"]     = true;
        filter[0]["race_status"]    = true;
        filter[0]["race_format_id"] = true;
        if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter)))
            return;

        // The heat id is null (newer servers) or 0 (older ones) when the timer is in Practice Mode rather
        // than on a heat; either way that's heat 0 here.
        JsonVariantConst heat;
        if (name[0] == 'r') {                   // race_status
            heat       = doc[1]["race_heat_id"];
            raceStatus = doc[1]["race_status"] | RH_RACE_READY;
            formatId   = doc[1]["race_format_id"] | -1;
        }
        else {                                  // current_heat
            heat       = doc[1]["current_heat"];
            raceStatus = RH_RACE_READY;         // a heat change means the previous race was saved or discarded
        }
        heatId   = heat.isNull() ? 0 : heat.as<int>();
        roundNum = doc[1]["next_round"] | -1;   // null while in practice mode
        Serial.printf("[RH] %s: heat %d, next round %d, race status %d\n", name, heatId, roundNum, raceStatus);
    }
    else if (!strcmp(name, "heat_list") || !strcmp(name, "heat_data")) {   // every heat with its name (heat_data: also broadcast on a rename)
        filter[0]["heats"][0]["id"] = true;
        filter[0]["heats"][0]["displayname"] = true;
        if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter)))
            return;
        readNames(doc[1]["heats"].as<JsonArrayConst>(), "displayname", heatNames);
        gotHeatList = true;
        Serial.printf("[RH] %s: %u heat names\n", name, (unsigned)heatNames.size());
    }
    else if (!strcmp(name, "format_data")) {    // every race format with its name
        filter[0]["formats"][0]["id"] = true;
        filter[0]["formats"][0]["name"] = true;
        if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter)))
            return;
        readNames(doc[1]["formats"].as<JsonArrayConst>(), "name", formatNames);
        gotFormatData = true;
        Serial.printf("[RH] format_data: %u format names\n", (unsigned)formatNames.size());
    }
    else
        return;                                 // heartbeat, leaderboard, ... - not ours

    if (gotHeatList && gotFormatData)
        listsRequestedAt = 0;                   // both lists in: nothing outstanding
    pending     = true;
    lastEventAt = millis();
}


//////////////////////////////////////////////////////////////////////////////
//
// Socket.IO client callback.
//

static void onSocketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case sIOtype_CONNECT:                   // the WebSocket is up
            lastRxAt = millis();
            nameHeatId = nameFormatId = -1;     // names may have changed while we were away: look them up afresh
            Serial.println("[RH] connected");
            sio.send(sIOtype_CONNECT, "/");     // join the default namespace (Socket.IO v3+ doesn't do this automatically)
            gotHeatList = gotFormatData = false;
            if (useApi) {
                sio.sendEVENT("[\"load_data\",{\"load_types\":[\"race_status\",\"current_heat\"]}]");
                listsRequestedAt = 0;
            }
            else {                              // socket first: the name lists as well
                sio.sendEVENT("[\"load_data\",{\"load_types\":[\"heat_list\",\"format_data\",\"race_status\",\"current_heat\"]}]");
                listsRequestedAt = millis();
            }
            connected = true;
            break;

        case sIOtype_DISCONNECT:
            if (connected)
                Serial.println("[RH] disconnected");
            connected = false;
            // Dropped right after asking for the name lists, before they arrived: they were too big for the
            // WebSockets library (it closes the socket on any frame over its limit, and doesn't say so).
            // Use /api for names from now on, and stop asking for the lists.
            if (!useApi && listsRequestedAt && millis() - listsRequestedAt < RH_LISTS_DROP_MS && !(gotHeatList && gotFormatData)) {
                useApi = true;
                listsRequestedAt = 0;
                Serial.println("[RH] name lists too large for the socket: using /api for names from now on");
            }
            break;

        case sIOtype_EVENT:
            lastRxAt = millis();                // every event counts as a sign of life, the 0.5 s heartbeat included
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
    lastRxAt = millis();
    linkLost = false;
    enabled = true;
}

void rhLoop() {
    if (!enabled)
        return;

    sio.loop();

    // Apply the received state once events have settled - and, right after a connect, once the name lists
    // have arrived or had their chance (otherwise the first update would needlessly go to /api).
    bool listsPending = listsRequestedAt && millis() - listsRequestedAt < RH_LISTS_WAIT_MS;
    if (pending && millis() - lastEventAt >= RH_SETTLE_MS && !listsPending) {
        pending = false;
        applyState();
    }

    bool lostNow = millis() - lastRxAt > RH_LINK_TIMEOUT_MS;
    if (lostNow != linkLost) {
        linkLost = lostNow;
        Serial.println(linkLost ? "[RH] link lost (no traffic from the server)" : "[RH] link active");
        if (linkLost && connected)
            sio.disconnect();                   // the socket may be half-open: drop it so the library reconnects
        updateDisplay();                        // the panel shows a marker while the link is lost
    }
}

bool rhConfigured() { return rhServer.length() > 0; }
bool rhConnected()  { return enabled && connected; }
bool rhLinkLost()   { return enabled && linkLost; }
bool rhUsingApi()   { return useApi; }
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
    return linkLost ? "lost" : "active";
}

String rhServerAddress() {
    return host + ":" + String(port);
}
