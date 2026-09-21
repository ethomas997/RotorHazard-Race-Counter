//////////////////////////////////////////////////////////////////////////////
//
// RotorHazard client module: follows a RotorHazard timer so the display tracks the current heat and round
// automatically. Connects to the server as an ordinary Socket.IO client (the same connection the browser
// pages use), listens for the race_status / current_heat broadcasts, looks up the heat name over the JSON
// API, and updates the counter. No server-side plugin or configuration is needed.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Arduino.h>

extern String rhServer;             // the RotorHazard server, "host" or "host:port" (port defaults to 5000); "" = not configured

String rhNormalizeServer(String s);  // tidy a server address typed by a user: trim, drop a leading http:// or https:// and a trailing /

void   rhBegin();                   // start following the server; call once WiFi is connected (does nothing if not configured)
void   rhLoop();                    // call from loop()

bool   rhConfigured();
bool   rhConnected();               // Socket.IO connection is up
String rhStatusText();              // "not configured" / "connecting" / "connected" - for the information screen
int    rhHeatId();                  // last heat id received from the server, -1 = none yet (0 = practice mode on the timer)
int    rhRound();                   // last round number received, -1 = none yet
String rhHeatName();                // display name of the last heat received ("" = none yet)
