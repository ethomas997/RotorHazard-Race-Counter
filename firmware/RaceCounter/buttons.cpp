//////////////////////////////////////////////////////////////////////////////
//
// Buttons module: debounced, edge-triggered handling of the DN / UP push-buttons.
//
//////////////////////////////////////////////////////////////////////////////

#include "RaceCounter.h"
#include "display.h"
#include "wifi_config.h"
#include "buttons.h"

struct Button {
    uint8_t         pin;
    bool            pressed;        // debounced state
    bool            lastRaw;        // last raw reading
    unsigned long   changedAt;      // when the raw reading last changed
};

static Button upButton = { UPSWITCH, false, false, 0 };
static Button dnButton = { DNSWITCH, false, false, 0 };


//////////////////////////////////////////////////////////////////////////////
//
// Configures the button pins. The buttons connect the pins to ground, so they read LOW when pressed.
//

void initButtons() {
    pinMode(UPSWITCH, INPUT_PULLUP);
    pinMode(DNSWITCH, INPUT_PULLUP);
}


//////////////////////////////////////////////////////////////////////////////
//
// Button handling.
//
// Presses are edge-triggered: one press is one action, and holding a button does not repeat. A single button
// acts as soon as it is released, or as soon as it has been held for BUTTON_COMBO_MS, whichever comes first -
// so a quick tap responds immediately instead of waiting out the combo window (and then being missed because
// the button was already up again). Both buttons down within BUTTON_COMBO_MS of each other is the
// both-buttons press, and it acts the moment the second one goes down. After any action nothing more happens
// until both buttons have been released.
//

// Returns the debounced state of a button (true = pressed, active low). A change in the raw reading has to
// hold for BUTTON_DEBOUNCE_MS before it counts, which filters contact bounce.

static bool readButton(Button &b) {
    bool raw = (digitalRead(b.pin) == LOW);

    if (raw != b.lastRaw) {
        b.lastRaw = raw;
        b.changedAt = millis();
    }
    else if (raw != b.pressed && millis() - b.changedAt >= BUTTON_DEBOUNCE_MS) {
        b.pressed = raw;
    }
    return b.pressed;
}

void handleButtons() {
    static unsigned long    firstDownAt = 0;            // when the first button went down; 0 = nothing pending
    static bool             seenUp = false;             // buttons seen down since firstDownAt
    static bool             seenDn = false;
    static bool             waitForRelease = false;     // set after an action: ignore the buttons until both are up

    bool up = readButton(upButton);
    bool dn = readButton(dnButton);

    if (waitForRelease) {
        if (!up && !dn)
            waitForRelease = false;
        return;
    }

    if (firstDownAt == 0) {                             // nothing pending: has a press started?
        if (!up && !dn)
            return;
        firstDownAt = millis();
        seenUp = up;
        seenDn = dn;
    }
    else {
        seenUp |= up;
        seenDn |= dn;
    }

    bool combo    = seenUp && seenDn;
    bool released = !up && !dn;
    bool expired  = millis() - firstDownAt >= BUTTON_COMBO_MS;

    if (!combo && !released && !expired)
        return;                                         // still waiting to see whether this becomes a both-buttons press

    firstDownAt = 0;
    waitForRelease = true;

    if (combo) {
        if (inAPMode)
            setStandAlone();                            // in AP mode both buttons turn on standalone mode - this will force a restart
        else
            doPractice();
    }
    else if (inAPMode) {
        // single buttons do nothing in AP mode
    }
    else if (seenUp) {
        Serial.println("Up Switch pressed!");

        if (++raceCount == 100)
            raceCount = 1;
        doRaceCount();
    }
    else if (raceCount != 0) {                          // Down does nothing while the splash screen is showing (race count 0)
        Serial.println("Down Switch pressed!");

        if (--raceCount == 0)
            raceCount = 99;
        doRaceCount();
    }
}
