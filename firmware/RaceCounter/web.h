//////////////////////////////////////////////////////////////////////////////
//
// Web module: the web interface and HTTP API served while connected to WiFi.
// The AP-mode setup page lives in wifi_config.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

// WebServer.h uses the unqualified name FS, which FS.h normally provides as a global alias - but Seeed_GFX
// (TFT_eSPI_ESP32_C3.h) defines FS_NO_GLOBALS before including FS.h, which suppresses that alias in any file
// that has already included TFT_eSPI.h. Provide it here so the include order never matters.
#include <FS.h>
using fs::FS;

#include <WebServer.h>

extern WebServer server;

void startWebServer();      // register the routes and start the server; call once WiFi is connected
void handleStatus();        // GET /status - device state as JSON (also registered by the AP-mode server)
