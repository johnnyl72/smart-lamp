/*
 * ============================================================================
 *  FRIENDSHIP LAMP
 * ============================================================================
 *
 *  Two lamps. Both run this identical firmware; DEVICE_ID in config.h ("A" or
 *  "B") is the only difference. Everyday light is the aurora: mint, cyan, sky,
 *  lavender and pink, with cool-white highlights. Happy uses the pastel
 *  rainbow, mellow deep blues, love warm pinks and the party the full
 *  saturated spectrum. Idle is dark. Every animation draws each LED on the
 *  ring separately, so light moves round it.
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
 *  FEELINGS AND PARTY (SECRETS_ENABLED) — a burst of quick taps
 *    3 taps             Happy     a fast, fluttering rainbow
 *    4 taps             Mellow    slow deep blues, with ice falling
 *    5 taps             Love      pink heartbeats
 *    6 taps             Party     all ten effects in turn, 10 s each, in an
 *                                 order shuffled by the show's seed
 *    7+ taps            nothing, so mashing the lamp sets nothing off
 *  Feelings and the party play on both lamps until a tap on either, or for
 *  SHOW_AUTO_OFF_MS at most. That tap ends the show on both and isn't sent
 *  as a tap. A run of taps just before a hold is dropped: it's a plain hold.
 *
 *  PRIORITY — one mode drives the LEDs at a time
 *      1 hold together   2 live hold (hers or yours)
 *      3 the answer      4 her tap waiting, or a feeling or party: these
 *                          share a level, so the newest wins
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
 *    {"type":"SECRET_GESTURE","id":"A-1a2b3c4d-13","fx":"love",
 *     "seed":2779096485,"at":1792566909123}
 *    {"type":"SHOW_END","id":"B-9f8e7d6c-5","of":"A-1a2b3c4d-13"}
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
 *  SHOWS — decided once, by the lamp that saw them
 *    The lamp whose sensor recognised a feeling or a party works out which
 *    show it is and publishes SECRET_GESTURE; the other lamp just plays it,
 *    so the two never disagree. "at" is a moment SECRET_LEAD_MS ahead on the
 *    shared NTP clock, so both lamps start together; one arriving
 *    SECRET_LATE_MS after that is dropped. "seed" makes both lamps' random
 *    patterns identical, the party's order of effects included. A show is
 *    only played here if it went out: offline, the lamp sends its tap spark
 *    instead.
 *
 *    A tap during a show ends it on both lamps. SHOW_END names the
 *    SECRET_GESTURE it ends, so one that crosses a newer show is ignored, and
 *    each lamp fades to what it rests on: dark, or its lamp light.
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
static const uint8_t PASTEL_COUNT = sizeof(PASTELS) / sizeof(PASTELS[0]);
// Everyday light: the cool end of the rainbow, plus pink.
static const Rgb AURORA[] = { PASTELS[3], PASTELS[4], PASTELS[5], PASTELS[6], PASTELS[0] };
static const uint8_t AURORA_COUNT = sizeof(AURORA) / sizeof(AURORA[0]);
static const uint8_t FLY_COLOURS[] = { 1, 3, 4 };  // her tap's fireflies: cyan, lavender, pink
static const Rgb PINK_WHITE = { 255, 222, 236 };   // love's heartbeats
static const Rgb ROSE       = { 255, 120, 170 };   // love: deeper than pink, so the beats show against it
static const Rgb LOVE[]     = { PASTELS[0], PASTELS[1], ROSE };
// Mellow: deep blue, periwinkle, soft cyan, sky and indigo.
static const Rgb MELLOW[]   = { { 70, 110, 255 }, { 120, 160, 255 }, PASTELS[4], PASTELS[5], { 150, 130, 255 } };
static const uint8_t MELLOW_COUNT = sizeof(MELLOW) / sizeof(MELLOW[0]);

// Party only: the whole spectrum, saturated. No pure red: saturated red
// flashing is the worst kind for photosensitive viewers.
static const Rgb SPECTRUM[] = {
    { 255,   0, 150 },   // 0 hot pink
    { 170,   0, 255 },   // 1 violet
    {  40,  60, 255 },   // 2 blue
    {   0, 200, 255 },   // 3 cyan
    {   0, 255, 140 },   // 4 green
    { 150, 255,   0 },   // 5 lime
    { 255, 200,   0 },   // 6 gold
    { 255, 110,   0 },   // 7 orange
};
static const uint8_t SPECTRUM_COUNT = sizeof(SPECTRUM) / sizeof(SPECTRUM[0]);

// Brightest any animation may get, as a fraction of MAX_BRIGHTNESS (which
// stays the USB power cap). Enforced once, per LED, in writeRing().
static const float MAX_LEVEL = 0.70f;

// LEDs respond linearly and eyes don't: without this, fades crawl at the top
// and drop off a cliff at the bottom. 2.6 is what Adafruit's gamma8() uses.
static const float LED_GAMMA = 2.6f;

// ============================================================================
//  TIMING
// ============================================================================

// Gesture recognition and the shows. Settable in config.h; see
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
#ifndef SECRETS_ENABLED
#define SECRETS_ENABLED 1
#endif
#ifndef SEQUENCE_WINDOW_MS
#define SEQUENCE_WINDOW_MS 2500
#endif
#ifndef LAMP_AUTO_OFF_MS
#define LAMP_AUTO_OFF_MS (5UL * 60UL * 60UL * 1000UL)
#endif
#ifndef SHOW_AUTO_OFF_MS
#define SHOW_AUTO_OFF_MS (5UL * 60UL * 1000UL)
#endif

// A run MASH_TAPS long or longer is mashing, and does nothing; before a hold
// it's dropped like any other run, leaving a plain hold.
static const uint8_t  MASH_TAPS            = 7;

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
static const uint32_t TRANSITION_MS        = 300;    // blend into a hold or a show
static const float    HOLD_TX_LEVEL        = 0.3f;   // your own lamp while you hold
static const uint32_t CURRENT_LAP_MS       = 3000;   // her hold's light, once round
static const uint32_t SHARED_WAVE_MS       = 4500;   // hold-together's waves, once round
static const uint8_t  SHARED_WAVES         = 3;      // white crests on the ring at once

static const uint32_t BURST_MS             = 600;    // happy bursting out of pixel 0
static const uint32_t HAPPY_TURN_MS        = 1800;   // happy's rainbow, once round
static const uint32_t HAPPY_DASH_MS        = 900;    // happy's white dash, once round
static const uint32_t MELLOW_TURN_MS       = 24000;  // mellow's blues, once round
static const uint32_t MELLOW_BREATHE_MS    = 7000;   // mellow, one slow breath
static const uint32_t MELLOW_SWELL_MS      = 16000;  // mellow's swell, once round
static const uint8_t  FALL_SPECKS          = 6;      // mellow's ice, falling at once
static const uint32_t LOVE_BEAT_MS         = 1200;   // love, one lub-dub
static const uint32_t LOVE_DRIFT_MS        = 9000;   // love's warm glow, once round
static const uint32_t PARTY_MS             = 10000;  // each party effect plays this long,
static const uint32_t PARTY_FADE_MS        = 800;    //   fading into the next over this
static const uint32_t CONFETTI_SLOT_MS     = 500;    // each LED's chance to pop comes round this often
// Bass line's beat: 128 bpm. The whole ring kicks once a beat, so never below
// 334 ms (3 flashes a second, the photosensitive-epilepsy limit).
static const uint32_t BASS_BEAT_MS         = 469;
static const uint32_t SECRET_LEAD_MS       = 250;    // shows start this far ahead, so hers can start with yours
static const uint32_t SECRET_LATE_MS       = 4000;   // a show arriving later than this is dropped

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
    // Shows: both lamps, until a tap or SHOW_AUTO_OFF_MS, then a fade. Keep
    // them last; isShow() relies on it.
    M_HAPPY, M_MELLOW, M_LOVE, M_PARTY
};

struct Show { const char* name; Mode mode; };
static const Show SHOWS[] = {
    { "happy",  M_HAPPY },
    { "mellow", M_MELLOW },
    { "love",   M_LOVE },
    { "party",  M_PARTY },
};
static const uint8_t SHOW_COUNT = sizeof(SHOWS) / sizeof(SHOWS[0]);

static bool isShow(Mode m) { return m >= M_HAPPY; }

static const Show* showFor(Mode m) {
    for (uint8_t k = 0; k < SHOW_COUNT; k++) if (SHOWS[k].mode == m) return &SHOWS[k];
    return nullptr;
}

static const Show* showNamed(const char* name) {
    for (uint8_t k = 0; k < SHOW_COUNT; k++) if (!strcmp(SHOWS[k].name, name)) return &SHOWS[k];
    return nullptr;
}

static uint8_t priorityOf(Mode m) {
    switch (m) {
        case M_SHARED:  return 1;
        case M_HOLD_RX:
        case M_HOLD_TX: return 2;
        case M_ACCEPT:  return 3;
        case M_WAITING: return 4;     // shares a level with every show: the newest wins
        case M_SENT:
        case M_IDLE:
        case M_FADE:
        case M_LAMP_ON:
        case M_LAMP:
        case M_LAMP_OFF: return 6;
        default:        return 4;     // every show
    }
}

static const char* modeName(Mode m) {
    if (isShow(m)) return showFor(m)->name;
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
static uint32_t modeStartedAt = 0;      // may be a moment ahead: see playShow()
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

// The show playing now. Both lamps get the same seed, so their "random"
// patterns match, and both know the id of the SECRET_GESTURE that started
// it, so a SHOW_END about an older show can't end this one.
static uint32_t showSeed = 0;
static String   showId;

// Touch.
static bool     touchRaw = false, touchStable = false, touchHeld = false;
static uint32_t touchChangedAt = 0, pressStartedAt = 0, lastReleaseAt = 0;
static uint32_t burstStartedAt = 0;     // first press of the current run of taps
static uint8_t  tapCount = 0;

// Message ids.
static uint32_t bootNonce = 0;
static uint32_t eventSeq  = 0;
static String   seenIds[16];
static uint8_t  seenNext  = 0;
static String   peerPrefix;             // "B-" on lamp A: her ids start with it

// Clock shared by both lamps, for hold-together and starting shows together.
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
//  SHOWS
// ============================================================================

// Starts a show at local time `startAt` unless something more important is
// showing. A show replaces a show, and her waiting tap: they never stack.
static bool playShow(Mode m, uint32_t startAt) {
    if (priorityOf(mode) < priorityOf(m)) {
        Serial.printf("[show] %s ignored: %s showing\n", showFor(m)->name, modeName(mode));
        return false;
    }
    setMode(m);
    modeStartedAt = startAt;            // a moment ahead, so both lamps start together
    Serial.printf("[show] playing %s\n", showFor(m)->name);
    return true;
}

// A feeling or party recognised on this lamp. Published first and only
// played if it went out, so an offline lamp never shows a shared moment
// alone; it sends its spark instead, like a tap.
static void launchShow(Mode m) {
    uint32_t seed = esp_random();
    JsonDocument doc;
    doc["type"] = "SECRET_GESTURE";
    doc["fx"]   = showFor(m)->name;
    doc["seed"] = seed;
    if (timeSynced) doc["at"] = syncedMillis() + SECRET_LEAD_MS;
    if (!publish(doc)) {
        tapFlashAt = millis();
        return;
    }
    showSeed = seed;
    showId   = doc["id"] | "";      // publish() stamped it
    playShow(m, millis() + (timeSynced ? SECRET_LEAD_MS : 0));
}

// A tap during a show ends it on both lamps: each fades to what it rests on,
// dark or its lamp light. The tap isn't also sent as a tap.
static void endShow() {
    Serial.printf("[tap] %s ended -> SHOW_END\n", modeName(mode));
    JsonDocument doc;
    doc["type"] = "SHOW_END";
    doc["of"]   = showId;
    publish(doc);
    setMode(M_FADE);
}

// ============================================================================
//  INTERACTIONS
// ============================================================================

// Her tap. Lower priority than any hold or answer, and not queued. It shares
// a level with the shows, so it replaces one, and a newer show replaces it.
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

// A show she recognised. She already decided which; this lamp just plays it,
// starting when she does.
static void receiveShow(JsonDocument& doc) {
    if (!SECRETS_ENABLED) {
        Serial.println("[show] ignored: shows are off on this lamp");
        return;
    }
    const Show* s = showNamed(doc["fx"] | "");
    if (!s) {
        Serial.println("[show] unknown show, ignoring");
        return;
    }
    uint32_t startAt = millis();
    uint64_t at = doc["at"] | (uint64_t)0;
    if (at && timeSynced) {
        int64_t ahead = (int64_t)at - (int64_t)syncedMillis();
        if (ahead < -(int64_t)SECRET_LATE_MS) {
            Serial.println("[show] arrived too late, dropped");
            return;
        }
        startAt += (int32_t)constrain(ahead, (int64_t)-1000, (int64_t)1000);   // late: join it partway
    }
    showSeed = doc["seed"] | 0U;
    showId   = doc["id"] | "";
    playShow(s->mode, startAt);
}

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
    } else if (!strcmp(type, "SECRET_GESTURE")) {
        Serial.printf("[mqtt] rx SECRET_GESTURE (%s)\n", doc["fx"] | "?");
        receiveShow(doc);
    } else if (!strcmp(type, "SHOW_END")) {
        // Only the show it names: one that crossed a newer show is stale.
        if (isShow(mode) && showId == (doc["of"] | "")) {
            Serial.printf("[mqtt] rx SHOW_END: %s ended\n", modeName(mode));
            setMode(M_FADE);
        } else {
            Serial.println("[mqtt] rx SHOW_END for a show not playing here, ignoring");
        }
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

// A heartbeat's shape over 0.6 s: quick up, slower down. 0 outside it.
static float pulse(float ms) {
    if (ms <= 0) return 0;
    if (ms < 120) return smooth(ms / 120);
    return 1 - smooth((ms - 120) / 480);
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

// Which of n, from a rand01() value. In float, the very top hashes round up
// to exactly 1.0, which would index one past the end.
static uint8_t nth(float r, uint8_t n) {
    uint8_t k = (uint8_t)(r * n);
    return k < n ? k : n - 1;
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

static Px rainbow(float h, float level) {
    return gradient(PASTELS, PASTEL_COUNT, h, level);
}

static Px spectrum(float h, float level) {
    return gradient(SPECTRUM, SPECTRUM_COUNT, h, level);
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

// --- Feelings: 3, 4 and 5 taps ----------------------------------------------
// They play until a tap, for minutes, so every phase is taken modulo its own
// period: sin() of a float clock that large has lost its precision.

// A burst out of pixel 0 that opens happy.
static float burstFront(uint32_t dt) {
    return (NUM_PIXELS / 2 + 2) * smooth((float)dt / BURST_MS);
}

static float burstReveal(uint8_t i, uint32_t dt) {
    return smooth((burstFront(dt) - fromStart(i)) / 2.0f);
}

static float burstEdge(uint8_t i, uint32_t dt) {
    return (1 - smooth((float)dt / BURST_MS)) * bump(fromStart(i) - burstFront(dt), 2.5f);
}

// Happy: the pastel rainbow spinning fast, every LED fluttering on its own
// quick beat, a white dash racing round and speckle everywhere. The flutters
// are out of step, so the ring as a whole never flashes.
static Px happy(uint8_t i, uint32_t dt, uint32_t seed) {
    float lit = burstReveal(i, dt);
    uint32_t beat = 180 + (uint32_t)(140 * rand01(i, seed));   // each LED's own flutter, in whole ms
    float flutter = 0.5f + 0.5f * sinf(TAU * ((float)(dt % beat) / beat + rand01(i, seed + 1)));
    Px p = rainbow((float)i / NUM_PIXELS - (float)(dt % HAPPY_TURN_MS) / HAPPY_TURN_MS, (0.3f + 0.4f * flutter) * lit);
    p = mix(p, px(COOL_WHITE, MAX_LEVEL), 0.7f * lit * glint(i, NUM_PIXELS * (float)(dt % HAPPY_DASH_MS) / HAPPY_DASH_MS));
    p = mix(p, px(COOL_WHITE, MAX_LEVEL), 0.9f * lit * sparkle(i, dt, 320, 120, 0.3f, seed + 2));
    return mix(p, px(COOL_WHITE, MAX_LEVEL), burstEdge(i, dt));
}

// Mellow: deep blues drifting slowly round and breathing, with icy speckles
// falling: each appears near pixel 0 and drifts down one side towards the
// far side of the ring, at its own pace, fading as it goes.
static Px mellow(uint8_t i, uint32_t dt, uint32_t seed) {
    float breathe = 0.5f - 0.5f * cosf(TAU * (float)(dt % MELLOW_BREATHE_MS) / MELLOW_BREATHE_MS);
    float swell = 0.5f + 0.5f * sinf(TAU * ((float)i / NUM_PIXELS - (float)(dt % MELLOW_SWELL_MS) / MELLOW_SWELL_MS));
    float level = (0.22f + 0.1f * breathe + 0.06f * swell) * smooth((float)dt / 2000);
    Px p = gradient(MELLOW, MELLOW_COUNT, (float)i / NUM_PIXELS + (float)(dt % MELLOW_TURN_MS) / MELLOW_TURN_MS, level);
    Px ice = mix(px(COOL_WHITE, MAX_LEVEL), px(PASTELS[4], MAX_LEVEL), 0.3f);
    for (uint8_t k = 0; k < FALL_SPECKS; k++) {
        uint32_t every = 4000 + (uint32_t)(3000 * rand01(k, seed));
        float u = (float)((dt + (uint32_t)(every * rand01(k, seed + 1))) % every) / every;
        float top = 3 * rand01(k, seed + 2);
        float y = top + (NUM_PIXELS / 2 + 1 - top) * u;
        uint8_t j = k % 2 == 0 ? i : (NUM_PIXELS - i) % NUM_PIXELS;   // every other one falls down the other side
        float seen = smooth(u * 6) * (1 - smooth((u - 0.55f) / 0.45f)) * smooth((float)dt / 1500);
        p = mix(p, ice, 0.75f * seen * min(1.0f, glint(j, y) + 0.5f * trail(j, y, 1.5f)));
    }
    return p;
}

// Love: a warm pink and peach glow, lub-dub heartbeats rippling out from
// pixel 0, and rose speckles.
static Px love(uint8_t i, uint32_t dt, uint32_t seed) {
    Px warm = gradient(LOVE, 3, (float)i / NUM_PIXELS + (float)(dt % LOVE_DRIFT_MS) / LOVE_DRIFT_MS,
                       0.26f * smooth((float)dt / 800));
    float beat = (float)(dt % LOVE_BEAT_MS) - fromStart(i) * 22;
    float thump = min(1.0f, pulse(beat) + 0.75f * pulse(beat - 260));
    Px p = mix(warm, px(PINK_WHITE, MAX_LEVEL), 0.8f * thump);
    return mix(p, px(ROSE, MAX_LEVEL), 0.8f * sparkle(i, dt, 1100, 380, 0.25f, seed));
}

// --- Party: 6 taps, all ten effects in turn, in the full spectrum -----------
// However wild, the whole ring changes brightness at most about twice a
// second; only single LEDs and moving lights go faster. Each effect runs from
// 0 to PARTY_MS, so none of them needs its phases wrapping.

// Prism: the spectrum spins, speeding up from 0.3 to 3 turns a second and back.
static Px prism(uint8_t i, uint32_t dt) {
    float s = dt / 1000.0f, T = PARTY_MS / 1000.0f;
    float turns = 0.3f * s + 1.35f * (s - T / TAU * sinf(TAU * s / T));
    Px p = spectrum((float)i / NUM_PIXELS - turns, 0.6f);
    return mix(p, px(COOL_WHITE, MAX_LEVEL), 0.6f * glint(i, NUM_PIXELS * turns));
}

// Comets: four comets at their own speeds, overtaking each other.
static Px comets(uint8_t i, uint32_t dt, uint32_t seed) {
    Px p = { 0, 0, 0 };
    uint8_t first = nth(rand01(0, seed + 2), SPECTRUM_COUNT);
    for (uint8_t k = 0; k < 4; k++) {
        float head = NUM_PIXELS * (rand01(k, seed + 1) + (float)dt / (700 + 1100 * rand01(k, seed)));
        Rgb c = SPECTRUM[(first + 2 * k) % SPECTRUM_COUNT];
        p = mix(p, px(c, MAX_LEVEL), trail(i, head, 4.0f));
        p = mix(p, px(COOL_WHITE, MAX_LEVEL), 0.8f * glint(i, head));
    }
    return p;
}

// Confetti: single LEDs pop in random colours, more and more of them.
static Px confetti(uint8_t i, uint32_t dt, uint32_t seed) {
    float chance = 0.12f + 0.6f * smooth((float)dt / 7000);
    uint32_t shifted = dt + (uint32_t)(rand01(i, seed) * CONFETTI_SLOT_MS);
    uint32_t slot = shifted / CONFETTI_SLOT_MS, into = shifted % CONFETTI_SLOT_MS;
    uint32_t roll = slot * 31 + i;
    Px p = spectrum((float)i / NUM_PIXELS + (float)dt / 4000, 0.1f);
    if (rand01(roll, seed + 1) >= chance) return p;
    Rgb c = SPECTRUM[nth(rand01(roll, seed + 2), SPECTRUM_COUNT)];
    return mix(p, px(c, MAX_LEVEL), smooth(into / 40.0f) * expf(-(float)into / 150));
}

// Fireworks: a white bang at a random spot, a shell of colour flying out both
// ways, then crackle inside it as it falls.
static Px fireworks(uint8_t i, uint32_t dt, uint32_t seed) {
    Px p = { 0, 0, 0 };
    for (uint8_t k = 0; k < 8; k++) {
        float u = ((float)dt - 200 - k * 1150 - 400 * rand01(k, seed)) / 1500;
        if (u <= 0 || u >= 1) continue;
        float d = apart(i, rand01(k, seed + 1) * NUM_PIXELS);
        Rgb c = SPECTRUM[nth(rand01(k, seed + 2), SPECTRUM_COUNT)];
        float r = 7 * (1 - (1 - u) * (1 - u));       // flies out fast, then slows
        float inside = smooth(r - d);
        p = mix(p, px(c, MAX_LEVEL), min(1.0f, bump(d - r, 1.8f) * (1 - u) + 0.5f * inside * (1 - u) * (1 - u)));
        p = mix(p, px(COOL_WHITE, MAX_LEVEL),
                0.9f * inside * smooth((u - 0.35f) / 0.2f) * (1 - u) * sparkle(i, dt, 160, 70, 0.45f, seed + 3 + k));
        p = mix(p, px(COOL_WHITE, MAX_LEVEL), (1 - smooth(u * 6)) * bump(d, 2.5f));   // the bang
    }
    return p;
}

// Plasma: three waves of colour flowing through each other, faster and faster.
static Px plasma(uint8_t i, uint32_t dt) {
    float at = (float)i / NUM_PIXELS, s = dt / 1000.0f;
    float go = s + 0.08f * s * s;
    float a = sinf(TAU * (at + go / 3));
    float b = sinf(TAU * (2 * at - go / 2.3f) + 1.1f);
    float c = sinf(TAU * (3 * at + go / 1.7f) + 2.3f);
    return spectrum(0.5f * a + 0.3f * b + go / 8, 0.35f + 0.15f * (c + 1));
}

// Ping-pong: two lights run out from pixel 0, meet on the far side, run back
// and meet again, a new colour and a little quicker each time, with a flash
// where they meet.
static Px pingPong(uint8_t i, uint32_t dt, uint32_t seed) {
    float s = dt / 1000.0f, ph = 1.1f * s + 0.07f * s * s;   // bounces so far
    uint32_t n = (uint32_t)ph;
    float f = ph - n;
    float x = (NUM_PIXELS / 2) * (n % 2 == 0 ? f : 1 - f);
    Rgb c = SPECTRUM[(n * 3 + seed % SPECTRUM_COUNT) % SPECTRUM_COUNT];
    float d = fromStart(i);
    Px p = mix(px(c, 0.12f), px(c, MAX_LEVEL), min(1.0f, bump(d - x, 2.0f) + 0.35f * bump(d - x, 5.0f)));
    float met = n % 2 == 0 ? 0 : NUM_PIXELS / 2;            // where the two last met
    return mix(p, px(COOL_WHITE, MAX_LEVEL), (1 - smooth(f * 5)) * bump(d - met, 2.5f));
}

// Raindrops: drops land on single LEDs and ripple out both ways, closer and
// closer together.
static Px raindrops(uint8_t i, uint32_t dt, uint32_t seed) {
    Px p = spectrum((float)i / NUM_PIXELS + (float)dt / 6000, 0.1f);
    for (uint8_t k = 0; k < 20; k++) {
        float at = 9000 * powf(k / 20.0f, 0.65f) + 250 * rand01(k, seed);
        float u = ((float)dt - at) / 1200;
        if (u <= 0 || u >= 1) continue;
        float d = apart(i, floorf(rand01(k, seed + 1) * NUM_PIXELS));
        Rgb c = SPECTRUM[nth(rand01(k, seed + 2), SPECTRUM_COUNT)];
        p = mix(p, px(c, MAX_LEVEL), bump(d - 6 * u, 1.5f) * (1 - u));
        p = mix(p, px(COOL_WHITE, MAX_LEVEL), (1 - smooth(u * 4)) * bump(d, 1.2f));   // the splash
    }
    return p;
}

// Bass line: 128 bpm. Each kick ripples out from pixel 0 and every LED jumps
// to its own height, the colours stepping round each beat, with hi-hats
// ticking on single LEDs.
static Px bassline(uint8_t i, uint32_t t, uint32_t seed) {
    uint32_t beat = t / BASS_BEAT_MS;
    float into = (float)(t % BASS_BEAT_MS) - fromStart(i) * 10;
    float kick = smooth(into / 25) * expf(-max(0.0f, into) / 170);
    float height = 0.3f + 0.7f * rand01(beat * 31 + i, seed);
    Px p = spectrum((float)i / NUM_PIXELS + (float)beat / 8, 0.12f + 0.58f * kick * height);
    float hat = sparkle(i, t + BASS_BEAT_MS / 2, BASS_BEAT_MS, 90, 0.25f, seed + 1);
    return mix(p, px(COOL_WHITE, MAX_LEVEL), 0.85f * hat);
}

// Colour wipes: each colour floods round over the last behind a white edge,
// quicker each time, every other one going the other way.
static Px wipes(uint8_t i, uint32_t dt) {
    float s = dt / 1000.0f, ph = 0.9f * s + 0.06f * s * s;   // wipes so far
    uint32_t n = (uint32_t)ph;
    float f = ph - n;
    float front = (NUM_PIXELS + 3) * f;                       // runs past the end, so it closes softly
    float at = n % 2 == 0 ? i : NUM_PIXELS - 1 - i;
    Px now = px(SPECTRUM[(n * 3) % SPECTRUM_COUNT], 0.6f);
    Px was = n > 0 ? px(SPECTRUM[(n * 3 + 5) % SPECTRUM_COUNT], 0.6f) : Px{ 0, 0, 0 };
    Px p = mix(was, now, smooth((front - at) / 1.5f));
    return mix(p, px(COOL_WHITE, MAX_LEVEL), bump(at - front, 1.5f));
}

// Marquee: eight lights, one in each spectrum colour, marching round,
// speeding up from 0.4 to 2.8 laps a second.
static Px marquee(uint8_t i, uint32_t dt) {
    float s = dt / 1000.0f, laps = 0.4f * s + 0.12f * s * s;
    Px p = { 0, 0, 0 };
    for (uint8_t k = 0; k < 8; k++)
        p = mix(p, px(SPECTRUM[k], MAX_LEVEL), bump(apart(i, NUM_PIXELS * (k / 8.0f + laps)), 1.5f));
    return p;
}

// Effect e of the ten, `t` ms into its 10 s run.
static Px partyEffect(uint8_t e, uint8_t i, uint32_t t, uint32_t seed) {
    switch (e) {
    case 0:  return prism(i, t);
    case 1:  return comets(i, t, seed);
    case 2:  return confetti(i, t, seed);
    case 3:  return fireworks(i, t, seed);
    case 4:  return plasma(i, t);
    case 5:  return pingPong(i, t, seed);
    case 6:  return raindrops(i, t, seed);
    case 7:  return bassline(i, t, seed);
    case 8:  return wipes(i, t);
    default: return marquee(i, t);
    }
}

// Which effect plays k-th. The order comes from the seed, so both lamps play
// the same one; stepping 3 or 7 round ten visits every effect before any
// repeats, and never plays one twice in a row.
static uint8_t partyPick(uint32_t k, uint32_t seed) {
    return (seed % 10 + k * (((seed >> 4) & 1) ? 3 : 7)) % 10;
}

// The party: all ten in turn, 10 s each, each fading into the next.
static Px party(uint8_t i, uint32_t dt, uint32_t seed) {
    uint32_t k = dt / PARTY_MS, t = dt % PARTY_MS;
    Px p = partyEffect(partyPick(k, seed), i, t, seed + k);
    if (k == 0 || t >= PARTY_FADE_MS) return p;
    Px was = partyEffect(partyPick(k - 1, seed), i, t + PARTY_MS, seed + k - 1);
    return mix(was, p, smooth((float)t / PARTY_FADE_MS));
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
    } else if (isShow(mode) && dt >= SHOW_AUTO_OFF_MS) {
        setMode(M_FADE);        // nobody tapped: one left playing in an empty room still ends
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
    case M_HAPPY:     return happy(i, dt, showSeed);
    case M_MELLOW:    return mellow(i, dt, showSeed);
    case M_LOVE:      return love(i, dt, showSeed);
    case M_PARTY:     return party(i, dt, showSeed);
    case M_LAMP_ON:   return lampIgnite(i, dt);
    case M_LAMP:      return lampGlow(i);
    case M_LAMP_OFF:  return lampCollapse(i, dt);
    case M_FADE:      return restGlow(i);       // to whatever the ring rests on
    default:          return { 0, 0, 0 };       // M_IDLE
    }
}

// How far pixel i has blended from what was showing into the current mode.
// Most modes take the whole ring at once. Her tap's bloom, the answer's flood
// and happy's burst sweep it in behind their moving edge instead, so a tap
// during her glow blooms over it rather than dimming it first.
static float entryWeight(uint8_t i, uint32_t dt) {
    switch (mode) {
    case M_WAITING: return min(1.0f, bloomReveal(i, dt) + bloomLight(i, dt));
    case M_ACCEPT:  return min(1.0f, acceptReveal(i, dt) + acceptEdge(i, dt));
    case M_HAPPY:   return min(1.0f, burstReveal(i, dt) + burstEdge(i, dt));
    case M_FADE:    return smooth((float)dt / FADE_OUT_MS);
    default:        return smooth((float)dt / TRANSITION_MS);
    }
}

static void renderFrame() {
    uint32_t now = millis();
    // A show can be set to start a moment ahead so both lamps start it
    // together. Until then the ring holds what it had.
    if ((int32_t)(now - modeStartedAt) >= 0) updateTimers(now);
    bool started = (int32_t)(now - modeStartedAt) >= 0;
    uint32_t dt = started ? now - modeStartedAt : 0;

    uint32_t ft = now - tapFlashAt;
    if (tapFlashAt && ft >= TAP_FLASH_MS) tapFlashAt = 0;

    for (uint8_t i = 0; i < NUM_PIXELS; i++) {
        // Blend from what was showing at the last mode change into this mode.
        Px out = started ? mix(from[i], targetFor(i, dt), entryWeight(i, dt)) : from[i];
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
// change. Ignored while a hold, a show or her tap is playing; your own tap's
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
    // Next, a tap ends a show on both lamps, and goes no further.
    if (n <= 2) {
        if (messageShowing()) { sendAck(); return; }
        if (isShow(mode))     { endShow(); return; }
    }
    if (n == 1) {
        sendTap();
        return;
    }
    if (n == 2) {
        toggleLamp();
        return;
    }
    if (!SECRETS_ENABLED || n >= MASH_TAPS) {
        Serial.printf("[tap] %u taps ignored\n", n);
        return;
    }
    Serial.printf("[tap] %u taps -> %s\n", n, n == 3 ? "happy" : n == 4 ? "mellow" : n == 5 ? "love" : "party");
    if (n == 3)      launchShow(M_HAPPY);
    else if (n == 4) launchShow(M_MELLOW);
    else if (n == 5) launchShow(M_LOVE);
    else             launchShow(M_PARTY);
}

static void serviceTouch() {
    uint32_t now = millis();
    bool raw = digitalRead(PIN_TOUCH) == HIGH;

    if (raw != touchRaw) { touchRaw = raw; touchChangedAt = now; }
    if (now - touchChangedAt >= DEBOUNCE_MS) touchStable = touchRaw;

    if (touchStable && !touchHeld) {                    // press
        touchHeld = true;
        pressStartedAt = now;
        if (tapCount == 0) burstStartedAt = now;
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
    // further touch, so a double tap can never also send a single tap, and a
    // feeling never sends the taps it's made of.
    if (tapCount > 0 && !touchHeld && now - lastReleaseAt >= DOUBLE_TAP_WINDOW_MS) {
        uint8_t n = tapCount;
        tapCount = 0;
        if (n > 2 && lastReleaseAt - burstStartedAt > SEQUENCE_WINDOW_MS)
            Serial.printf("[tap] %u taps too spread out for a feeling, ignored\n", n);
        else
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
    } else if (line.startsWith("TAPS ")) {  // = n quick taps
        dispatchTaps((uint8_t)constrain(line.substring(5).toInt(), 1, 9));
    } else if (line == "HOLD") {            // = finger down
        if (!holdActive) startLocalHold();
    } else if (line == "RELEASE") {         // = finger up
        if (holdActive && !touchHeld) endLocalHold();
    } else if (line == "LAMP") {            // = double tap with nothing waiting
        toggleLamp();
    } else if (line.startsWith("SHOW ")) {  // = any show, as if recognised here
        const Show* s = showNamed(line.substring(5).c_str());
        if (s) launchShow(s->mode);
        else Serial.println("[show] no such show: happy, mellow, love or party");
    } else if (line == "STATUS") {
        Serial.printf("id=%s mode=%s wifi=%d mqtt=%d rssi=%d synced=%d\n",
                      DEVICE_ID, modeName(mode), WiFi.status() == WL_CONNECTED,
                      mqtt.connected(), WiFi.RSSI(), timeSynced);
        Serial.printf("   holding=%d spent=%d peerHold=%d messageShowing=%d\n",
                      holdActive, holdSpent, peerHoldAlive(), messageShowing());
        Serial.printf("   lamp=%d secrets=%d\n", lampOn, SECRETS_ENABLED);
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

    // UTC only: the clock just keeps hold-together and the shows in step.
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
