/*
 * ============================================================================
 *  FRIENDSHIP LAMP
 * ============================================================================
 *
 *  Two lamps. Both run this identical firmware; DEVICE_ID in config.h ("A" or
 *  "B") is the only difference. Everyday light is the aurora: mint, cyan, sky,
 *  lavender and pink, with cool-white highlights. Idle is dark. Every
 *  animation draws each LED on the ring separately, so light moves round it.
 *
 *  GESTURES — on your own lamp
 *    Tap            "I miss you". On her lamp aurora light blooms both ways
 *                   round the ring from one spot, then sinks to a low shimmer
 *                   with three fireflies circling at their own speeds. It
 *                   waits TAP_WAIT_MS for her to tap back, then fades out. On
 *                   yours a spark runs once round to show the tap registered,
 *                   then one dim firefly circles while you wait. Neither says
 *                   the tap arrived. Another tap blooms again and restarts
 *                   both waits; animations never stack.
 *    Tap back       "Miss you too". A tap (or a double tap) while her tap is
 *                   waiting or fading answers it: bright aurora floods both
 *                   lamps, then dims down to dark, or to the lamp light. This
 *                   is the only confirmation that a tap reached her.
 *    Double tap,    Lamp light. With nothing waiting, a double tap turns this
 *    nothing waiting  lamp into a light to live by: a white point bursts round
 *                   the ring like a supernova and opens into an aura of soft
 *                   cool pastels drifting slowly round. Another double tap
 *                   collapses it back into the point and out. Hers doesn't
 *                   change. Anything that plays meanwhile goes back to the
 *                   aura, not to dark, and it turns itself off after
 *                   LAMP_AUTO_OFF_MS.
 *    Hold           "I'm here with you". On her lamp a white light glides
 *                   round the ring trailing aurora and stardust for as long
 *                   as you hold, then fades out over 3 s when you let go.
 *                   Yours glows softly to show the hold is live.
 *
 *  GESTURES — together
 *    Hold together  "We're connected". Both lamps glow in aurora with three
 *                   white waves racing round them, in step and sparkling,
 *                   while you both hold. Either letting go fades both out
 *                   over 3 s. Nothing resumes.
 *
 *  PRIORITY — one mode drives the LEDs at a time
 *      1 hold together   2 live hold (hers or yours)
 *      3 the answer      4 her tap waiting
 *      6 your tap waiting, idle, the lamp light (and fades): anything may
 *        interrupt these
 *    A lower-priority event never interrupts a higher one and is not queued.
 *    A higher one replaces a lower one outright, and so does a newer one on
 *    the same level: whatever it replaced is dropped, never resumed. Every
 *    change of mode blends from what's actually on the ring, so transitions
 *    are smooth.
 *
 *  PROTOCOL
 *    lamp/<PAIR_ID>/<ID>/signal    this lamp publishes, the other subscribes
 *    lamp/<PAIR_ID>/<ID>/status    "online" / "offline" (last will), retained
 *
 *    {"type":"TAP","id":"A-1a2b3c4d-7"}
 *    {"type":"ACK","id":"B-9f8e7d6c-3","of":"A-1a2b3c4d-7"}
 *    {"type":"HOLD_START","id":"A-1a2b3c4d-8","hold":"A-1a2b3c4d-8"}
 *    {"type":"HOLD_END","id":"A-1a2b3c4d-12","hold":"A-1a2b3c4d-8"}
 *
 *    Nothing on the signal topic is retained: these are moments, not state.
 *    A lamp that was unplugged simply misses them and boots dark.
 *
 *    Every message has a unique id (device, a per-boot random nonce, a counter)
 *    and the receiver drops ids it has already seen, so a redelivery can't
 *    restart an animation. An id that doesn't start with the other lamp's
 *    DEVICE_ID isn't hers and is ignored.
 *
 *    A hold repeats HOLD_START every second with the same "hold" id. The
 *    receiver only starts the glow for a hold id it isn't already showing, so
 *    the repeats just keep it alive. If they stop for HOLD_TIMEOUT_MS — her
 *    power cut, her WiFi, a lost HOLD_END — the receiver ends the hold itself.
 *    It also ends it at once if the broker says her lamp went offline or this
 *    lamp loses its own connection.
 *
 *  HOLD TOGETHER — no coordinator
 *    Each lamp sees "my finger is down" and "her hold is live" and decides
 *    alone. Both see the same two facts, so both agree. When either lets go,
 *    the other lamp's still-held hold is spent: it sends HOLD_END and stops
 *    repeating, and the lamp that let go ignores the rest of that hold id.
 *    Otherwise the remaining finger would carry on as a solo hold and light
 *    the other lamp straight back up.
 *
 *    The waves take their position from the NTP clock, so both lamps show the
 *    same frame. The answer's flood starts when each lamp hears the tap back,
 *    so the two line up to within network latency.
 *
 *  WIFI
 *    If the saved network can't be joined at power-up, the lamp opens the
 *    "Lamp Setup" portal by itself (first boot, new router). No touch can
 *    erase credentials; only the serial RESET_WIFI does that.
 *
 *  NON-BLOCKING
 *    There is no delay() anywhere below setup() except RESET_WIFI, which runs
 *    immediately before ESP.restart(). Everything is millis()-driven.
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>
#include <esp_system.h>
#include <time.h>

#include "config.h"

// ============================================================================
//  HARDWARE
// ============================================================================
// These GPIOs match the Linked Lamp enclosure's cable channels.

static const uint8_t PIN_TOUCH  = 4;    // TTP223 I/O pin
static const uint8_t PIN_PIXELS = 27;   // WS2812B data in

// RING_PIXELS lives in config.h — COUNT THE LEDs on the ring that actually
// arrives. Wrong count = dark pixels or an out-of-range write.
static const uint8_t NUM_PIXELS = RING_PIXELS;

// ============================================================================
//  LOOK
// ============================================================================

// These are the colours as a screen would show them; writeRing()
// gamma-corrects them for the LEDs.
struct Rgb { uint8_t r, g, b; };
static const Rgb COOL_WHITE = { 235, 247, 255 };   // highlights, moments of connection

// A pastel rainbow, in order round the colour wheel. Soft enough that even
// the whitest of them draws less than cool white.
static const Rgb PASTELS[] = {
    { 255, 176, 214 },   // 0 pink
    { 255, 204, 176 },   // 1 peach
    { 255, 236, 170 },   // 2 butter
    { 170, 245, 200 },   // 3 mint
    { 150, 238, 245 },   // 4 soft cyan
    { 160, 205, 255 },   // 5 sky blue
    { 200, 178, 255 },   // 6 lavender
};
// Everyday light: the cool end of the rainbow, plus pink.
static const Rgb AURORA[] = { PASTELS[3], PASTELS[4], PASTELS[5], PASTELS[6], PASTELS[0] };
static const uint8_t AURORA_COUNT = sizeof(AURORA) / sizeof(AURORA[0]);
static const uint8_t FLY_COLOURS[] = { 1, 3, 4 };  // her tap's fireflies: cyan, lavender, pink

// Brightest any animation may get, as a fraction of MAX_BRIGHTNESS (which
// stays the USB power cap). Enforced once, per LED, in writeRing().
static const float MAX_LEVEL = 0.70f;

// LEDs respond linearly and eyes don't: without this, fades crawl at the top
// and drop off a cliff at the bottom. 2.6 is what Adafruit's gamma8() uses.
static const float LED_GAMMA = 2.6f;

// ============================================================================
//  TIMING
// ============================================================================

// Gesture recognition. Settable in config.h; see
// config.example.h for how to choose them. Defaults here keep an older
// config.h compiling.
#ifndef HOLD_THRESHOLD_MS
#define HOLD_THRESHOLD_MS 250
#endif
#ifndef DOUBLE_TAP_WINDOW_MS
#define DOUBLE_TAP_WINDOW_MS 300
#endif
#ifndef TAP_WAIT_MS
#define TAP_WAIT_MS 120000
#endif
#ifndef LAMP_AUTO_OFF_MS
#define LAMP_AUTO_OFF_MS (5UL * 60UL * 60UL * 1000UL)
#endif

static const uint32_t DEBOUNCE_MS          = 40;

static const uint32_t BLOOM_GATHER_MS      = 350;    // her tap's highlight brightens in one spot,
static const uint32_t BLOOM_SPREAD_MS      = 1400;   //   then opens out round the ring,
static const uint32_t BLOOM_MS             = BLOOM_GATHER_MS + BLOOM_SPREAD_MS;
static const uint32_t SETTLE_MS            = 1800;   //   and its glow sinks to the waiting shimmer
static const uint32_t WAIT_BREATHE_MS      = 6000;   // the shimmer, one slow breath
static const uint32_t FLY_LAPS[]           = { 4200, 6100, 8700 };  // her tap's fireflies, once round each
static const uint32_t SENT_LAP_MS          = 6000;   // your own waiting firefly, once round
static const uint32_t ACCEPT_SWEEP_MS      = 800;    // the answer floods round in this,
static const uint32_t ACCEPT_MS            = 3400;   //   then glows, dimming, until the fade
static const uint32_t FADE_OUT_MS          = 3000;   // every fade to dark
static const uint32_t TAP_FLASH_MS         = 900;    // your tap's spark, once round
static const uint32_t TRANSITION_MS        = 300;    // blend into a hold
static const float    HOLD_TX_LEVEL        = 0.3f;   // your own lamp while you hold
static const uint32_t CURRENT_LAP_MS       = 3000;   // her hold's light, once round
static const uint32_t SHARED_WAVE_MS       = 4500;   // hold-together's waves, once round
static const uint8_t  SHARED_WAVES         = 3;      // white crests on the ring at once

static const uint32_t HOLD_HEARTBEAT_MS    = 1000;   // HOLD_START repeat while holding
static const uint32_t HOLD_TIMEOUT_MS      = 3000;   // no repeat for this long = hold over

static const uint32_t FRAME_MS             = 16;     // ~60fps render tick

static const uint32_t MQTT_RETRY_MS        = 5000;

// ============================================================================
//  STATE
// ============================================================================

enum Mode : uint8_t {
    M_IDLE,       // dark
    M_WAITING,    // her tap: a bloom sinking to a shimmer with fireflies, until answered or TAP_WAIT_MS
    M_SENT,       // your tap: one firefly circling while you wait for her answer
    M_ACCEPT,     // a tap answered: aurora floods both lamps, then dims
    M_HOLD_TX,    // you're holding: a soft glow
    M_HOLD_RX,    // she's holding: a light gliding round
    M_SHARED,     // both holding: waves in step
    M_FADE,       // fading to dark, or to the lamp light; anything may interrupt
    M_LAMP_ON,    // lamp light: a white supernova opening into the aura
    M_LAMP,       // lamp light: the aura, until a double tap or LAMP_AUTO_OFF_MS
    M_LAMP_OFF,   // lamp light: collapsing back into a point, then dark
};

static uint8_t priorityOf(Mode m) {
    switch (m) {
        case M_SHARED:  return 1;
        case M_HOLD_RX:
        case M_HOLD_TX: return 2;
        case M_ACCEPT:  return 3;
        case M_WAITING: return 4;
        default:        return 6;     // your tap waiting, idle, the lamp light, fades
    }
}

static const char* modeName(Mode m) {
    switch (m) {
        case M_IDLE: return "idle";        case M_WAITING: return "waiting";
        case M_SENT: return "sent";        case M_ACCEPT: return "answered";
        case M_HOLD_TX: return "holding";  case M_HOLD_RX: return "her-hold";
        case M_SHARED: return "together";  case M_LAMP_ON: return "lamp-on";
        case M_LAMP: return "lamp";        case M_LAMP_OFF: return "lamp-off";
        default: return "fade";
    }
}

// Linear light, before gamma: channel * level, 0..255.
struct Px { float r, g, b; };
static Px px(Rgb c, float level) { return { c.r * level, c.g * level, c.b * level }; }
static Px mix(Px a, Px b, float t) {
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

static Mode     mode = M_IDLE;
static uint32_t modeStartedAt = 0;
static Px       shown[NUM_PIXELS];      // what's on the ring right now
static Px       from[NUM_PIXELS];       // what was on it when the mode changed

static uint32_t messageLitAt   = 0;     // last tap from her: TAP_WAIT_MS counts from here
static bool     fadingMessage  = false; // M_FADE is the end of her tap (a tap back still answers)
static String   lastRxTapId;            // the tap an answer acknowledges
static uint32_t messageSeed    = 0;     // where her tap's fireflies start and its twinkles fall
static uint32_t tapFlashAt     = 0;     // local "tap registered" spark

// The lamp light. On, it's what the ring rests on instead of dark.
static bool     lampOn   = false;
static uint32_t lampOnAt = 0;

// Your hold.
static bool     holdActive = false;     // past the threshold (finger or serial HOLD)
static bool     holdSpent  = false;     // ended a hold-together; ignored until release
static String   localHoldId;
static uint32_t lastHeartbeatAt = 0;

// Her hold. Read through peerHoldAlive(), never the flag alone.
static bool     peerHold = false;
static String   peerHoldId;
static String   spentPeerHoldId;        // her hold that already ended a hold-together
static uint32_t peerHoldSeenAt = 0;

// Touch.
static bool     touchRaw = false, touchStable = false, touchHeld = false;
static uint32_t touchChangedAt = 0, pressStartedAt = 0, lastReleaseAt = 0;
static uint8_t  tapCount = 0;

// Message ids.
static uint32_t bootNonce = 0;
static uint32_t eventSeq  = 0;
static String   seenIds[16];
static uint8_t  seenNext  = 0;
static String   peerPrefix;             // "B-" on lamp A: her ids start with it

// Clock shared by both lamps, for hold-together.
static bool     timeSynced   = false;
static uint32_t epochAtSync  = 0;
static uint32_t millisAtSync = 0;

// Connectivity.
static uint32_t wifiDownSince = 0;
static uint32_t lastMqttAttempt = 0;
static uint32_t lastFrameAt = 0;

static String topicSignalSelf, topicSignalPeer, topicStatusSelf, topicStatusPeer;

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_PIXELS, NEO_GRB + NEO_KHZ800);
WiFiClientSecure  net;
PubSubClient      mqtt(net);
WiFiManager       wm;

// ============================================================================
//  HELPERS
// ============================================================================

static uint64_t syncedMillis() {
    if (!timeSynced) return millis();   // lamps drift apart; nobody sees both rooms
    return (uint64_t)epochAtSync * 1000ULL + (millis() - millisAtSync);
}

// Every mode change starts from what's actually showing, so nothing snaps.
static void setMode(Mode m) {
    mode = m;
    modeStartedAt = millis();
    memcpy(from, shown, sizeof(shown));
    fadingMessage = false;
}

static bool messageShowing() {
    return mode == M_WAITING || (mode == M_FADE && fadingMessage);
}

// Her hold counts only while its repeats keep arriving and we can hear them.
static bool peerHoldAlive() {
    return peerHold && mqtt.connected() && millis() - peerHoldSeenAt < HOLD_TIMEOUT_MS;
}

static bool localHoldLive() {
    return holdActive && !holdSpent;
}

static String newEventId() {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s-%08lx-%lu", DEVICE_ID,
             (unsigned long)bootNonce, (unsigned long)++eventSeq);
    return String(buf);
}

// True if this id arrived before. Remembers the last 16.
static bool alreadySeen(const char* id) {
    if (!id || !*id) return false;
    for (uint8_t i = 0; i < 16; i++) if (seenIds[i] == id) return true;
    seenIds[seenNext] = id;
    seenNext = (seenNext + 1) % 16;
    return false;
}

// Gamma and MAX_BRIGHTNESS in one step, rounded once. gamma8() followed by
// setBrightness() truncated twice, which blacked out the dim end of every
// fade and trail.
static uint8_t toLed(float v) {
    float lin = powf(constrain(v, 0.0f, 255.0f) / 255.0f, LED_GAMMA);
    return (uint8_t)(lin * MAX_BRIGHTNESS + 0.5f);
}

// The only place pixels are written. Caps each LED's brightest channel at
// MAX_LEVEL so every animation obeys the same limit.
static void writeRing(const Px* ring) {
    const float cap = 255.0f * MAX_LEVEL;
    for (uint8_t i = 0; i < NUM_PIXELS; i++) {
        Px p = ring[i];
        float top = max(p.r, max(p.g, p.b));
        if (top > cap) { float k = cap / top; p.r *= k; p.g *= k; p.b *= k; }
        pixels.setPixelColor(i, toLed(p.r), toLed(p.g), toLed(p.b));
    }
    pixels.show();
}

// ============================================================================
//  PUBLISHING
// ============================================================================

// Stamps a fresh id on `doc` and sends it. Never retained: a stale gesture
// replayed to a lamp that boots later is exactly what this protocol exists to
// avoid.
static bool publish(JsonDocument& doc, bool log = true) {
    const char* type = doc["type"] | "?";
    if (!mqtt.connected()) {
        if (log) Serial.printf("[mqtt] offline, %s not sent\n", type);
        return false;
    }
    doc["id"] = newEventId();
    char buf[224];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    bool ok = mqtt.publish(topicSignalSelf.c_str(), (const uint8_t*)buf, n, false);
    if (log) Serial.printf("[mqtt] tx %s -> %s\n", buf, ok ? "ok" : "FAILED");
    return ok;
}

// HOLD_START (first and every repeat) or HOLD_END for our hold.
static bool publishHold(const char* type, bool log = true) {
    JsonDocument doc;
    doc["type"] = type;
    doc["hold"] = localHoldId;
    return publish(doc, log);
}

// ============================================================================
//  INTERACTIONS
// ============================================================================

// Her tap. Lower priority than any hold or answer, and not queued.
static void receiveTap(const char* id) {
    if (priorityOf(mode) < priorityOf(M_WAITING)) {
        Serial.printf("[lamp] tap ignored: %s showing\n", modeName(mode));
        return;
    }
    lastRxTapId = id;
    bool restart = messageShowing();
    messageLitAt = millis();
    // Blooms again over whatever is showing, and the wait restarts. Except
    // while the last bloom is still opening: restarting would freeze it
    // halfway round, so that one just gets the fresh wait.
    bool opening = mode == M_WAITING && millis() - modeStartedAt < BLOOM_MS;
    if (!opening) {
        setMode(M_WAITING);
        messageSeed = esp_random();
    }
    Serial.printf("[lamp] lighting for %lus (tap from peer%s)\n",
                  (unsigned long)(TAP_WAIT_MS / 1000), restart ? ", restarted" : "");
}

// The answer, on this lamp, unless something more important is showing.
static void startAccept() {
    if (priorityOf(mode) < priorityOf(M_ACCEPT) || mode == M_ACCEPT) {
        Serial.printf("[lamp] answer ignored: %s showing\n", modeName(mode));
        return;
    }
    Serial.println("[lamp] answered: flooding");
    setMode(M_ACCEPT);
}

// Her tap back, answering ours.
static void receiveAck() {
    startAccept();
}

static void sendTap() {
    Serial.println("[tap] single -> TAP");
    JsonDocument doc;
    doc["type"] = "TAP";
    publish(doc);
    // Sent or not: the spark means "registered" and the firefly "waiting",
    // never "delivered". Only her answer says it arrived.
    tapFlashAt = millis();
    if (priorityOf(mode) >= priorityOf(M_SENT)) setMode(M_SENT);
}

static void sendAck() {
    Serial.println("[tap] answer -> ACK");
    JsonDocument doc;
    doc["type"] = "ACK";
    doc["of"]   = lastRxTapId;
    publish(doc);
    startAccept();
}

static void startLocalHold() {
    holdActive = true;
    holdSpent  = false;
    localHoldId = newEventId();
    Serial.println("[tap] hold -> HOLD_START");
    publishHold("HOLD_START");
    lastHeartbeatAt = millis();
}

static void endLocalHold() {
    if (!holdSpent) {
        Serial.println("[tap] release -> HOLD_END");
        publishHold("HOLD_END");
    }
    holdActive = false;
    holdSpent  = false;
}

// Either finger lifted during hold-together. Both lamps fade, and neither
// hold may carry on as a solo hold: that would relight the other lamp.
static void endHoldTogether() {
    Serial.println("[lamp] hold together ended");
    if (holdActive && !holdSpent) {
        holdSpent = true;
        publishHold("HOLD_END");
    }
    if (peerHold) {
        spentPeerHoldId = peerHoldId;
        peerHold = false;
    }
    setMode(M_FADE);
}

// Holds outrank everything else, so they're decided every loop from the two
// facts both lamps can see: my finger, her hold.
static void updateHolds() {
    bool mine = localHoldLive();
    bool hers = peerHoldAlive();

    if (peerHold && !hers) {
        Serial.println("[lamp] peer hold ended (released, timed out or offline)");
        peerHold = false;
    }

    if (mode == M_SHARED) {
        if (!mine || !hers) endHoldTogether();
        return;
    }
    if (mine && hers) {
        Serial.println("[lamp] holding together");
        setMode(M_SHARED);
        return;
    }
    if (mode == M_HOLD_RX && !hers) { setMode(M_FADE); return; }
    if (mode == M_HOLD_TX && !mine) { setMode(M_FADE); return; }

    if (hers && mode != M_HOLD_RX && priorityOf(mode) > 2) setMode(M_HOLD_RX);
    else if (mine && mode != M_HOLD_TX && priorityOf(mode) > 2) setMode(M_HOLD_TX);
}

// While holding, repeat HOLD_START so her lamp knows the hold is still live.
static void serviceHoldHeartbeat() {
    if (!localHoldLive()) return;
    uint32_t now = millis();
    if (now - lastHeartbeatAt < HOLD_HEARTBEAT_MS) return;
    lastHeartbeatAt = now;
    publishHold("HOLD_START", false);
}

// ============================================================================
//  INBOUND
// ============================================================================

static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
    if (topicStatusPeer == topic) {
        bool online = (len == 6 && !memcmp(payload, "online", 6));
        Serial.printf("[mqtt] peer %s\n", online ? "online" : "offline");
        if (!online) peerHold = false;
        return;
    }
    if (len == 0) return;       // a retained message being cleared

    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) {
        Serial.println("[mqtt] rx malformed json, ignoring");
        return;
    }
    const char* type = doc["type"] | "";
    const char* id   = doc["id"]   | "";
    // Only the paired lamp publishes here, and its ids start with its DEVICE_ID.
    if (strncmp(id, peerPrefix.c_str(), peerPrefix.length())) {
        Serial.println("[mqtt] rx id not from the paired lamp, ignoring");
        return;
    }
    if (alreadySeen(id)) return;    // redelivered: never retrigger

    if (!strcmp(type, "TAP")) {
        Serial.println("[mqtt] rx TAP");
        receiveTap(id);
    } else if (!strcmp(type, "ACK")) {
        Serial.println("[mqtt] rx ACK");
        receiveAck();
    } else if (!strcmp(type, "HOLD_START")) {
        const char* hold = doc["hold"] | "";
        if (spentPeerHoldId == hold) return;    // it already ended a hold-together
        if (!peerHold || peerHoldId != hold) Serial.println("[mqtt] rx HOLD_START");
        peerHold = true;
        peerHoldId = hold;
        peerHoldSeenAt = millis();
    } else if (!strcmp(type, "HOLD_END")) {
        const char* hold = doc["hold"] | "";
        Serial.println("[mqtt] rx HOLD_END");
        if (peerHoldId == hold) peerHold = false;
        if (spentPeerHoldId == hold) spentPeerHoldId = "";
    }
    // Anything else (including the old firmware's messages) is ignored.
}

// ============================================================================
//  ANIMATION  (called ~60x/sec, never blocks, never allocates)
// ============================================================================
//  Every mode draws each LED separately. Positions round the ring are in
//  fractional pixels, so light glides between LEDs instead of hopping, and
//  most things that travel go the same way: up the pixel numbers.

static const float TAU             = 6.28318531f;  // one full turn, in radians
static const float HEAD_PX         = 2.0f;   // a moving light's half-width; under ~1.5
                                             // it visibly hops from LED to LED
static const float CURRENT_TAIL_PX = 4.0f;   // her hold's trail
static const uint32_t ACCEPT_SEED  = 21;         // the answer's glints: the same on both

static float smooth(float t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return t * t * (3 - 2 * t);
}

// A soft bell: 1 at x = 0, falling to 0 at |x| = half.
static float bump(float x, float half) {
    return smooth(1.0f - fabsf(x) / half);
}

// How far pixel i is behind a light at `head`, going backwards round the ring.
static float behind(uint8_t i, float head) {
    float d = fmodf(head - i, (float)NUM_PIXELS);
    return d < 0 ? d + NUM_PIXELS : d;
}

// How far pixel i is from `pos` round the ring, the short way.
static float apart(uint8_t i, float pos) {
    float d = behind(i, pos);
    return min(d, NUM_PIXELS - d);
}

// How far pixel i is from pixel 0, either way round. Anything that opens out
// from pixel 0 in both directions only needs this.
static float fromStart(uint8_t i) {
    return (float)(i <= NUM_PIXELS / 2 ? i : NUM_PIXELS - i);
}

// The moving light itself: a soft bell a few LEDs wide. As it moves, one LED
// brightens while its neighbour dims, so the total stays steady and the light
// glides between LEDs instead of blinking from one to the next.
static float glint(uint8_t i, float head) {
    return bump(apart(i, head), HEAD_PX);
}

// The trail behind a moving light. Nothing under the light itself, which
// glint() draws: if both lit the pixel it's arriving at, their blends would
// compound and that edge would snap on. Grows over the light's half-width,
// dies away over about `tail` pixels, and is cut off `length` pixels back.
static float trail(uint8_t i, float head, float tail, float length = NUM_PIXELS) {
    float d = behind(i, head);
    return smooth(d / HEAD_PX) * expf(-d / tail) * smooth(length - d);
}

// Cheap, well-mixed integer hash. The same inputs give the same "random"
// numbers on both lamps, which is what keeps their patterns identical.
static uint32_t hashBits(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static float rand01(uint32_t a, uint32_t b) {
    return hashBits(a * 0x9E3779B9U + hashBits(b)) / 4294967296.0f;
}

// Now and then a pixel sparkles, rising and falling over `len` ms. Which
// pixels and when follows from `seed` and the clock alone, so two lamps on
// the same clock sparkle alike. `chance` is how often each pixel's turn,
// every `every` ms, actually sparkles.
static float sparkle(uint8_t i, uint32_t t, uint32_t every, uint32_t len, float chance, uint32_t seed) {
    uint32_t shifted = t + (uint32_t)(rand01(i, seed) * every);
    uint32_t into = shifted % every;
    if (into >= len || rand01(shifted / every * 31 + i, seed + 1) >= chance) return 0;
    float u = (float)into / len;
    return smooth(u * 2) * (1 - smooth(u * 2 - 1));
}

// A colour on a looped gradient through `stops`; h in turns, any value.
static Px gradient(const Rgb* stops, uint8_t n, float h, float level) {
    h -= floorf(h);
    float x = h * n;
    uint8_t a = (uint8_t)x % n;
    return mix(px(stops[a], level), px(stops[(a + 1) % n], level), x - floorf(x));
}

// The aurora at pixel i: curtains of colour drifting at different speeds.
static Px auroraAt(uint8_t i, float t, float level) {
    float at = (float)i / NUM_PIXELS;
    float a = sinf(TAU * (at + t / 11000));
    float b = sinf(TAU * (2 * at - t / 17000) + 1.3f);
    return gradient(AURORA, AURORA_COUNT, 0.35f + 0.25f * a + 0.15f * b, level);
}

// --- Your tap: the spark ----------------------------------------------------
// The ring brightens a little and a spark leaves pixel 0, runs once round
// through the aurora's colours and is gone, as if the tap were setting off for
// her lamp. Drawn over whatever is showing; returns how much of it covers
// pixel i.
static float sendSpark(uint8_t i, uint32_t ft) {
    float u = (float)ft / TAP_FLASH_MS;
    float swell = 0.45f * smooth(u * 8) * (1 - smooth(u * 2));
    // Eases in and out: any faster and it hops LEDs instead of gliding.
    float spark = smooth(u * 8) * (1 - smooth(u * 1.25f - 0.25f)) * glint(i, NUM_PIXELS * smooth(u));
    return max(swell, spark);
}

// --- Her tap: the bloom -----------------------------------------------------
// A highlight gathers at pixel 0, splits and runs both ways round the ring
// revealing what's behind it, and fades as the two halves meet on the far
// side. The halves mirror each other, so all that matters is how far a pixel
// is from pixel 0.

static float bloomSpread(uint32_t dt) {         // 0..1 through the spread
    return constrain(((float)dt - BLOOM_GATHER_MS) / BLOOM_SPREAD_MS, 0.0f, 1.0f);
}

// How far out from pixel 0 it has reached: quick, then easing off. It runs
// past the far side so the halves close softly instead of stopping dead.
static float bloomFront(uint32_t dt) {
    float s = 1 - bloomSpread(dt);
    return (NUM_PIXELS / 2 + 2) * (1 - s * s);
}

static float bloomPeak(uint32_t dt) {
    if (dt < BLOOM_GATHER_MS) return smooth((float)dt / BLOOM_GATHER_MS);
    float s = bloomSpread(dt);
    return 1 - s * s;
}

static float bloomLight(uint8_t i, uint32_t dt) {
    return bloomPeak(dt) * bump(fromStart(i) - bloomFront(dt), 2.0f);
}

static float bloomReveal(uint8_t i, uint32_t dt) {    // 1 once it has passed pixel i
    return smooth((bloomFront(dt) - fromStart(i)) / 2.0f);
}

// The bloom's highlight over whatever it reveals: white at its brightest,
// aurora as it spreads and fades.
static Px bloomOver(Px base, uint8_t i, uint32_t dt) {
    Px highlight = mix(auroraAt(i, dt, MAX_LEVEL), px(COOL_WHITE, MAX_LEVEL), bloomPeak(dt));
    return mix(base, highlight, bloomLight(i, dt));
}

// --- Her tap, waiting for your answer ---------------------------------------
// The bloom leaves bright aurora that sinks to a low shimmer, and three
// fireflies circle at their own speeds, each breathing on its own beat, with
// now and then a single LED twinkling white. `rest` is what it sinks to: the
// shimmer, or the lamp light if it's on, so the room doesn't go dim.

static Px waitingRest(uint8_t i, uint32_t dt) {
    return auroraAt(i, dt, 0.24f + 0.04f * cosf(TAU * (float)dt / WAIT_BREATHE_MS));
}

static Px waiting(uint8_t i, uint32_t dt, uint32_t seed, Px rest) {
    float settle = 1 - smooth(((float)dt - BLOOM_MS) / SETTLE_MS);
    Px p = mix(rest, auroraAt(i, dt, 0.62f), settle);
    float out = smooth(((float)dt - BLOOM_MS) / 800);    // the fireflies come out as the bloom ends
    for (uint8_t k = 0; k < 3; k++) {
        float head = NUM_PIXELS * (rand01(k, seed) + (float)dt / FLY_LAPS[k]);
        float glow = out * (0.75f + 0.25f * sinf(TAU * (float)dt / (1300 + 500 * k) + k));
        Px c = px(AURORA[FLY_COLOURS[k]], MAX_LEVEL);
        p = mix(p, c, glow * trail(i, head, 2.0f));
        p = mix(p, mix(c, px(COOL_WHITE, MAX_LEVEL), 0.5f), glow * glint(i, head));
    }
    p = mix(p, px(COOL_WHITE, MAX_LEVEL), 0.7f * out * sparkle(i, dt, 1700, 500, 0.12f, seed + 5));
    return bloomOver(p, i, dt);
}

// --- Your tap, waiting for hers ---------------------------------------------
// One dim firefly circling, setting off as your spark finishes (540 ms in),
// its colour drifting slowly through the aurora.
static Px sentGlow(uint8_t i, uint32_t dt, Px rest) {
    float head = NUM_PIXELS * (float)dt / SENT_LAP_MS;
    float glow = 0.5f + 0.12f * sinf(TAU * (float)dt / 2400);
    Px c = gradient(AURORA, AURORA_COUNT, (float)dt / 20000, glow);
    float out = smooth(((float)dt - 540) / 600);
    Px p = mix(rest, c, out * trail(i, head, 2.5f));
    return mix(p, mix(c, px(COOL_WHITE, glow), 0.4f), out * glint(i, head));
}

// --- The answer: aurora floods both lamps -----------------------------------
// Bright aurora floods both ways round from pixel 0 behind a white edge, runs
// at three times the waiting pace with glints everywhere, then dims down
// before the fade takes it the rest of the way.

static float acceptFront(uint32_t dt) {
    return (NUM_PIXELS / 2 + 2) * smooth((float)dt / ACCEPT_SWEEP_MS);
}

static float acceptReveal(uint8_t i, uint32_t dt) {   // 1 once the flood has passed pixel i
    return smooth((acceptFront(dt) - fromStart(i)) / 2.0f);
}

static float acceptEdge(uint8_t i, uint32_t dt) {
    return (1 - smooth((float)dt / ACCEPT_SWEEP_MS)) * bump(fromStart(i) - acceptFront(dt), 2.5f);
}

static Px accepted(uint8_t i, uint32_t dt) {
    float level = MAX_LEVEL - 0.3f * smooth(((float)dt - 1500) / (ACCEPT_MS - 1500));
    Px p = auroraAt(i, 3.0f * dt, level);
    p = mix(p, px(COOL_WHITE, MAX_LEVEL), 0.8f * sparkle(i, dt, 700, 260, 0.35f, ACCEPT_SEED));
    return mix(p, px(COOL_WHITE, MAX_LEVEL), acceptEdge(i, dt));
}

// --- Her hold: the living current -------------------------------------------
// A white light gliding round over a faint aurora, trailing the aurora's
// colours and the odd sparkle of stardust in its wake.
static Px livingCurrent(uint8_t i, uint32_t dt) {
    float head = NUM_PIXELS * (float)(dt % CURRENT_LAP_MS) / CURRENT_LAP_MS;
    Px p = auroraAt(i, dt, 0.2f);
    p = mix(p, gradient(AURORA, AURORA_COUNT, (float)dt / 9000 + behind(i, head) / NUM_PIXELS, MAX_LEVEL),
            trail(i, head, CURRENT_TAIL_PX));
    float dust = sparkle(i, dt, 900, 450, 0.4f, 7) * smooth(1 - behind(i, head) / 10);
    p = mix(p, px(COOL_WHITE, MAX_LEVEL), 0.55f * dust);
    return mix(p, px(COOL_WHITE, MAX_LEVEL), glint(i, head));
}

// Your own lamp while you hold: a soft glow, so you know the hold is live.
static Px holdingGlow(uint8_t i, uint32_t dt) {
    return auroraAt(i, dt, HOLD_TX_LEVEL);
}

// --- Hold together: the shared glow -----------------------------------------
// Aurora turning one way and three white waves racing the other, sparkling at
// the crests and now and then in between.
// Everything comes from the shared clock, so both lamps draw the same frame.
static Px sharedGlow(uint8_t i) {
    uint64_t t  = syncedMillis();
    float at    = (float)i / NUM_PIXELS;
    float waves = (float)(t % SHARED_WAVE_MS) / SHARED_WAVE_MS;
    float drift = (float)(t % (4 * SHARED_WAVE_MS)) / (4 * SHARED_WAVE_MS);
    float crest = 0.5f + 0.5f * cosf(TAU * SHARED_WAVES * (at - waves));
    crest *= crest * crest;                 // narrow crests, a wide calm between
    Px p = mix(gradient(AURORA, AURORA_COUNT, at + drift, 0.5f), px(COOL_WHITE, MAX_LEVEL), crest);
    float s = max(sparkle(i, (uint32_t)t, 900, 350, 0.5f, 11) * smooth(crest * 3),
                  0.6f * sparkle(i, (uint32_t)t, 1300, 300, 0.15f, 12));
    return mix(p, px(COOL_WHITE, MAX_LEVEL), 0.85f * s);
}

// --- Lamp light: a double tap with nothing waiting --------------------------
// An aura of soft cool pastels drifting slowly round: two bands of brightness
// travelling opposite ways at different speeds, so it never sits still and
// never quite repeats. Always between 40% and 70%, bright enough to live by.
// It runs on its own clock from switch-on, so whatever plays over it fades
// back to the aura exactly where it would have been.
//
// It lights like a supernova: a white point gathers at pixel 0 and bursts
// both ways round the ring, the aura opening up behind it. Turning it off
// runs that backwards: the aura falls back into the point and goes out.
static const uint32_t LAMP_ON_MS  = 2000;
static const uint32_t LAMP_OFF_MS = 2200;

static Px lampGlow(uint8_t i) {
    float at = (float)i / NUM_PIXELS, t = (float)(millis() - lampOnAt);
    float a = sinf(TAU * (at - t / 12000));
    float b = sinf(TAU * (2 * at + t / 20000) + 1.7f);
    float hue = 0.4f + 0.3f * sinf(TAU * (at + t / 30000)) + 0.1f * b;
    return gradient(AURORA, AURORA_COUNT, hue, 0.55f + 0.1f * a + 0.05f * b);
}

static Px lampIgnite(uint8_t i, uint32_t dt) {
    const float t = dt, a = fromStart(i), reach = NUM_PIXELS / 2 + 2;
    if (t < 500) return px(COOL_WHITE, MAX_LEVEL * smooth(t / 500) * bump(a, 2.0f));
    float u = min(1.0f, (t - 500) / 1100), s = 1 - u;
    float front = reach * (1 - s * s * s);
    float lit = smooth((front - a) / 2);
    // White just behind the burst, turning into the aura further back.
    float settled = max(smooth((front - a) / 6), smooth((t - 1600) / 400));
    Px p = mix(px(COOL_WHITE, MAX_LEVEL * lit), mix(Px{ 0, 0, 0 }, lampGlow(i), lit), settled);
    return mix(p, px(COOL_WHITE, MAX_LEVEL), (1 - u) * bump(a - front, 2.5f));   // the burst's edge
}

static Px lampCollapse(uint8_t i, uint32_t dt) {
    const float t = dt, a = fromStart(i), reach = NUM_PIXELS / 2 + 2;
    float u = min(1.0f, t / 1600);
    float front = reach * (1 - smooth(u));
    Px p = mix(Px{ 0, 0, 0 }, lampGlow(i), smooth((front - a) / 2));
    float core = smooth(u) * (1 - smooth((t - 1600) / 600));
    return mix(p, px(COOL_WHITE, MAX_LEVEL), core * bump(a, 2.0f));
}

// What the ring rests on: the lamp light while it's on, otherwise dark.
static Px restGlow(uint8_t i) {
    return lampOn ? lampGlow(i) : Px{ 0, 0, 0 };
}

// End timed modes before drawing.
static void updateTimers(uint32_t now) {
    uint32_t dt = now - modeStartedAt;
    if (mode == M_WAITING && now - messageLitAt >= TAP_WAIT_MS) {
        setMode(M_FADE);
        fadingMessage = true;   // a tap back during the fade still answers
    } else if (mode == M_SENT && dt >= TAP_WAIT_MS) {
        setMode(M_FADE);
    } else if (mode == M_ACCEPT && dt >= ACCEPT_MS) {
        setMode(M_FADE);        // the flood's last glow, out over 3 s
    } else if (mode == M_FADE && dt >= FADE_OUT_MS) {
        setMode(lampOn ? M_LAMP : M_IDLE);
    } else if (mode == M_LAMP_ON && dt >= LAMP_ON_MS) {
        setMode(M_LAMP);
    } else if (mode == M_LAMP_OFF && dt >= LAMP_OFF_MS) {
        setMode(M_IDLE);
    }
    // A duration, not a time of day: so a forgotten light still ends dark.
    if (lampOn && LAMP_AUTO_OFF_MS && now - lampOnAt >= LAMP_AUTO_OFF_MS) {
        Serial.println("[lamp] lamp light off (auto)");
        lampOn = false;
        if (mode == M_LAMP) setMode(M_LAMP_OFF);
    }
}

static Px targetFor(uint8_t i, uint32_t dt) {
    switch (mode) {
    case M_WAITING:   return waiting(i, dt, messageSeed, lampOn ? lampGlow(i) : waitingRest(i, dt));
    case M_SENT:      return sentGlow(i, dt, restGlow(i));
    case M_ACCEPT:    return accepted(i, dt);
    case M_HOLD_TX:   return holdingGlow(i, dt);
    case M_HOLD_RX:   return livingCurrent(i, dt);
    case M_SHARED:    return sharedGlow(i);
    case M_LAMP_ON:   return lampIgnite(i, dt);
    case M_LAMP:      return lampGlow(i);
    case M_LAMP_OFF:  return lampCollapse(i, dt);
    case M_FADE:      return restGlow(i);       // to whatever the ring rests on
    default:          return { 0, 0, 0 };       // M_IDLE
    }
}

// How far pixel i has blended from what was showing into the current mode.
// Most modes take the whole ring at once. Her tap's bloom and the answer's
// flood sweep it in behind their moving edge instead, so a tap during her
// glow blooms over it rather than dimming it first.
static float entryWeight(uint8_t i, uint32_t dt) {
    switch (mode) {
    case M_WAITING: return min(1.0f, bloomReveal(i, dt) + bloomLight(i, dt));
    case M_ACCEPT:  return min(1.0f, acceptReveal(i, dt) + acceptEdge(i, dt));
    case M_FADE:    return smooth((float)dt / FADE_OUT_MS);
    default:        return smooth((float)dt / TRANSITION_MS);
    }
}

static void renderFrame() {
    uint32_t now = millis();
    updateTimers(now);
    uint32_t dt = now - modeStartedAt;

    uint32_t ft = now - tapFlashAt;
    if (tapFlashAt && ft >= TAP_FLASH_MS) tapFlashAt = 0;

    for (uint8_t i = 0; i < NUM_PIXELS; i++) {
        // Blend from what was showing at the last mode change into this mode.
        Px out = mix(from[i], targetFor(i, dt), entryWeight(i, dt));
        // Your own tap: its spark, over whatever is showing.
        if (tapFlashAt) out = mix(out, gradient(AURORA, AURORA_COUNT, (float)i / NUM_PIXELS, MAX_LEVEL), sendSpark(i, ft));
        shown[i] = out;
    }
    writeRing(shown);
}

// ============================================================================
//  TOUCH
// ============================================================================

// The lamp light: a double tap with nothing to answer turns this lamp into an
// ordinary lamp, and another turns it off again. Local only: hers doesn't
// change. Ignored while a hold or her tap is playing; your own tap's
// waiting firefly gives way to it.
static void toggleLamp() {
    if (priorityOf(mode) < priorityOf(M_LAMP)) {
        Serial.printf("[tap] double ignored: %s showing\n", modeName(mode));
        return;
    }
    lampOn = !lampOn;
    lampOnAt = millis();
    Serial.printf("[tap] double -> lamp light %s\n", lampOn ? "on" : "off");
    setMode(lampOn ? M_LAMP_ON : M_LAMP_OFF);
}

// A run of quick taps, finished.
static void dispatchTaps(uint8_t n) {
    // Her tap comes first, always: one tap answers it, and so does an
    // excited double tap, which would otherwise be lost to the lamp light.
    if (n <= 2 && messageShowing()) { sendAck(); return; }
    if (n == 1) {
        sendTap();
        return;
    }
    if (n == 2) {
        toggleLamp();
        return;
    }
    Serial.printf("[tap] %u taps ignored\n", n);
}

static void serviceTouch() {
    uint32_t now = millis();
    bool raw = digitalRead(PIN_TOUCH) == HIGH;

    if (raw != touchRaw) { touchRaw = raw; touchChangedAt = now; }
    if (now - touchChangedAt >= DEBOUNCE_MS) touchStable = touchRaw;

    if (touchStable && !touchHeld) {                    // press
        touchHeld = true;
        pressStartedAt = now;
    } else if (touchStable && touchHeld) {              // holding
        if (!holdActive && now - pressStartedAt >= HOLD_THRESHOLD_MS) {
            tapCount = 0;           // taps just before a hold are dropped: it's a plain hold
            startLocalHold();
        }
    } else if (!touchStable && touchHeld) {             // release
        touchHeld = false;
        if (holdActive) endLocalHold();
        else { tapCount++; lastReleaseAt = now; }
    }

    // Taps are only decided once the double-tap window has passed with no
    // further touch, so a double tap can never also send a single tap.
    if (tapCount > 0 && !touchHeld && now - lastReleaseAt >= DOUBLE_TAP_WINDOW_MS) {
        uint8_t n = tapCount;
        tapCount = 0;
        dispatchTaps(n);
    }
}

// ============================================================================
//  CONNECTIVITY
// ============================================================================

static void serviceWifi() {
    if (WiFi.status() == WL_CONNECTED) { wifiDownSince = 0; return; }

    if (!wifiDownSince) {
        wifiDownSince = millis();
        Serial.println("[wifi] link down, reconnecting");
        WiFi.reconnect();
    }
}

static void serviceMqtt() {
    if (mqtt.connected()) {
        mqtt.loop();
        return;
    }
    if (WiFi.status() != WL_CONNECTED) return;

    uint32_t now = millis();
    if (now - lastMqttAttempt < MQTT_RETRY_MS) return;
    lastMqttAttempt = now;

    String cid = String("lamp-") + DEVICE_ID + "-" +
                 String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);

    Serial.printf("[mqtt] connecting as %s ... ", cid.c_str());

    // LWT: if we drop off ungracefully the broker publishes "offline" for us.
    bool ok = mqtt.connect(cid.c_str(), MQTT_USER, MQTT_PASS,
                           topicStatusSelf.c_str(), 1, true, "offline");

    if (ok) {
        Serial.println("connected");
        mqtt.publish(topicStatusSelf.c_str(), "online", true);
        // Clear anything the old firmware left retained on our signal topic,
        // so the other lamp never replays it.
        mqtt.publish(topicSignalSelf.c_str(), (const uint8_t*)"", 0, true);
        mqtt.subscribe(topicSignalPeer.c_str(), 1);
        mqtt.subscribe(topicStatusPeer.c_str(), 1);
        lastHeartbeatAt = 0;    // still holding? tell her straight away
    } else {
        // rc=-2 is almost always the root CA. rc=4/5 is bad credentials.
        Serial.printf("failed, rc=%d\n", mqtt.state());
    }
}

static void serviceTime() {
    if (timeSynced) return;
    time_t t = time(nullptr);
    if (t > 1700000000) {
        epochAtSync  = (uint32_t)t;
        millisAtSync = millis();
        timeSynced   = true;    // set last — syncedMillis() reads the latches
        Serial.printf("[ntp] synced: %lu\n", (unsigned long)epochAtSync);
    }
}

// Typing into the serial monitor stands in for the touch sensor on the bench.
static void serviceSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "RESET_WIFI") {
        wm.resetSettings(); delay(200); ESP.restart();
    } else if (line == "PING") {            // = single tap
        dispatchTaps(1);
    } else if (line == "ACK") {             // = double tap
        dispatchTaps(2);
    } else if (line == "HOLD") {            // = finger down
        if (!holdActive) startLocalHold();
    } else if (line == "RELEASE") {         // = finger up
        if (holdActive && !touchHeld) endLocalHold();
    } else if (line == "LAMP") {            // = double tap with nothing waiting
        toggleLamp();
    } else if (line == "STATUS") {
        Serial.printf("id=%s mode=%s wifi=%d mqtt=%d rssi=%d synced=%d\n",
                      DEVICE_ID, modeName(mode), WiFi.status() == WL_CONNECTED,
                      mqtt.connected(), WiFi.RSSI(), timeSynced);
        Serial.printf("   holding=%d spent=%d peerHold=%d messageShowing=%d\n",
                      holdActive, holdSpent, peerHoldAlive(), messageShowing());
        Serial.printf("   lamp=%d\n", lampOn);
    }
}

// ============================================================================
//  SETUP / LOOP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n\n=== Friendship Lamp %s (%s) ===\n", DEVICE_ID, OWNER_NAME);

    pinMode(PIN_TOUCH, INPUT);

    // Dark from power-up: nothing from before the reboot is resurrected. No
    // setBrightness(): writeRing() applies MAX_BRIGHTNESS itself, after gamma.
    pixels.begin();
    pixels.clear();
    pixels.show();

    bootNonce = esp_random();

    String peer = (strcmp(DEVICE_ID, "A") == 0) ? "B" : "A";
    peerPrefix = peer + "-";
    topicSignalSelf = String("lamp/") + PAIR_ID + "/" + DEVICE_ID + "/signal";
    topicSignalPeer = String("lamp/") + PAIR_ID + "/" + peer + "/signal";
    topicStatusSelf = String("lamp/") + PAIR_ID + "/" + DEVICE_ID + "/status";
    topicStatusPeer = String("lamp/") + PAIR_ID + "/" + peer + "/status";
    Serial.printf("[mqtt] tx %s\n[mqtt] rx %s\n",
                  topicSignalSelf.c_str(), topicSignalPeer.c_str());

    wm.setConfigPortalTimeout(300);
    wm.setConnectTimeout(20);

    // Blocking whenever the saved network can't be joined — first boot, after
    // a credential wipe, or when her router has changed: opens an AP called
    // "Lamp Setup" with a captive portal. The ring stays dark in here.
    if (!wm.autoConnect("Lamp Setup")) {
        Serial.println("[wifi] portal timed out — rebooting");
        ESP.restart();
    }
    Serial.printf("[wifi] connected, ip=%s rssi=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());

    // UTC only: the clock just keeps hold-together in step.
    configTime(0, 0, NTP_SERVER);

    net.setCACert(MQTT_ROOT_CA);
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMqttMessage);
    mqtt.setBufferSize(512);
    mqtt.setKeepAlive(30);
}

void loop() {
    serviceWifi();
    serviceMqtt();
    serviceTime();
    serviceTouch();
    serviceSerial();
    serviceHoldHeartbeat();
    updateHolds();

    uint32_t now = millis();
    if (now - lastFrameAt >= FRAME_MS) {
        lastFrameAt = now;
        renderFrame();
    }
}
