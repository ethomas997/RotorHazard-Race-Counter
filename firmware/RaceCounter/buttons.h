//////////////////////////////////////////////////////////////////////////////
//
// Buttons module: debounced, edge-triggered handling of the DN / UP push-buttons.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

void initButtons();         // configure the button pins; call once from setup()
void handleButtons();       // poll the buttons and act on presses; call from loop()
