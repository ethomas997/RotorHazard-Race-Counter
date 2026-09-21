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
// Presses are edge-triggered: one press is one action. After any action nothing more happens until both
// buttons have been released - except that holding a single button keeps stepping the race number, as if
// it were being pressed repeatedly (one step per BUTTON_REPEAT_MS, in practice one per panel refresh).
//
// A single button acts as soon as it is released, or as soon as it has been held for BUTTON_COMBO_MS,
// whichever comes first - so a quick tap responds immediately instead of waiting out the combo window (and
// then being missed because the button was already up again).
//
// Both buttons down within BUTTON_COMBO_MS of each other is a both-buttons press. If either is released
// before BUTTON_LONG_MS it is a short press (toggle practice mode; in AP mode, set standalone mode); if both
// are still held at BUTTON_LONG_MS it is a long press, which shows the information screen.
//
// While the information screen is showing, any press just restores the screen that was there before.
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

static void singlePress(bool isUp) {
    if (inAPMode)
        return;                                         // single buttons do nothing in AP mode

    if (isUp) {
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

static void shortBothPress() {
    if (inAPMode)
        setStandAlone();                                // in AP mode both buttons turn on standalone mode - this will force a restart
    else
        doPractice();
}

static void longBothPress() {
    Serial.println("Both switches held: information screen");
    showInfoScreen();
}

void handleButtons() {
    enum State { IDLE, PENDING, REPEAT, BOTH, WAIT_RELEASE };

    static State            state = IDLE;
    static unsigned long    stateSince = 0;             // when the current state was entered
    static bool             seenUp = false;             // buttons seen down since the press started
    static bool             seenDn = false;
    static bool             repeatUp = false;           // which button is being held in REPEAT
    static unsigned long    lastRepeat = 0;             // when the held button last stepped the count

    bool up = readButton(upButton);
    bool dn = readButton(dnButton);
    unsigned long now = millis();

    switch (state) {

    case IDLE:
        if (!up && !dn)
            break;
        if (infoScreenShowing()) {                      // any press on the information screen just goes back
            restorePreviousScreen();
            state = WAIT_RELEASE;
            break;
        }
        seenUp = up;
        seenDn = dn;
        stateSince = now;
        state = PENDING;
        // fall through - the press may already be a both-buttons press

    case PENDING:                                       // one button is down: is the other coming?
        seenUp |= up;
        seenDn |= dn;
        if (seenUp && seenDn) {
            stateSince = now;                           // the long-press timer starts when both are down
            state = BOTH;
        }
        else if (!up && !dn) {                          // released: a tap
            singlePress(seenUp);
            state = WAIT_RELEASE;
        }
        else if (now - stateSince >= BUTTON_COMBO_MS) { // still held: act now and keep repeating while it stays down
            singlePress(seenUp);
            repeatUp = seenUp;
            lastRepeat = millis();
            state = inAPMode ? WAIT_RELEASE : REPEAT;   // (single buttons do nothing in AP mode, so nothing to repeat)
        }
        break;

    case REPEAT:                                        // a single button is being held: step again every BUTTON_REPEAT_MS
        if (up && dn) {                                 // the other button came down too: stop, and ignore it
            state = WAIT_RELEASE;
        }
        else if (!(repeatUp ? up : dn)) {               // the held button was released
            state = (up || dn) ? WAIT_RELEASE : IDLE;   // (if the other one is down by now, ignore it until released)
        }
        else if (now - lastRepeat >= BUTTON_REPEAT_MS) {
            singlePress(repeatUp);
            lastRepeat = millis();                      // (millis(), not now: the panel refresh inside singlePress takes seconds)
        }
        break;

    case BOTH:                                          // both were down: short or long press?
        if (!(up && dn)) {
            shortBothPress();
            state = WAIT_RELEASE;
        }
        else if (now - stateSince >= BUTTON_LONG_MS) {
            longBothPress();
            state = WAIT_RELEASE;
        }
        break;

    case WAIT_RELEASE:                                  // an action has happened: ignore everything until both are up
        if (!up && !dn)
            state = IDLE;
        break;
    }
}
