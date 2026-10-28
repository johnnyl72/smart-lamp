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
 *                   glows for TAP_WAIT_MS, then fades out. On yours a spark
 *                   runs once round to show the tap registered, not that it
 *                   arrived. Another tap blooms again and restarts the wait;
 *                   animations never stack.
 *
 *  MODES — one mode drives the LEDs at a time
 *    Every change of mode blends from what's actually on the ring, so
 *    transitions are smooth.
 *
 *  PROTOCOL
 *    lamp/<PAIR_ID>/<ID>/signal    this lamp publishes, the other subscribes
 *    lamp/<PAIR_ID>/<ID>/status    "online" / "offline" (last will), retained
 *
 *    {"type":"TAP","id":"A-1a2b3c4d-7"}
 *
 *    Nothing on the signal topic is retained: these are moments, not state.
 *    A lamp that was unplugged simply misses them and boots dark.
 *
 *    Every message has a unique id: device, a per-boot random nonce, a counter.
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
#ifndef TAP_WAIT_MS
#define TAP_WAIT_MS 120000
#endif

static const uint32_t DEBOUNCE_MS          = 40;

static const uint32_t BLOOM_GATHER_MS      = 350;    // her tap's highlight brightens in one spot,
static const uint32_t BLOOM_SPREAD_MS      = 1400;   //   then opens out round the ring,
static const uint32_t BLOOM_MS             = BLOOM_GATHER_MS + BLOOM_SPREAD_MS;
static const uint32_t SETTLE_MS            = 1800;   //   and its glow sinks to the waiting shimmer
static const uint32_t WAIT_BREATHE_MS      = 6000;   // the shimmer, one slow breath
static const uint32_t FLY_LAPS[]           = { 4200, 6100, 8700 };  // her tap's fireflies, once round each
static const uint32_t FADE_OUT_MS          = 3000;   // every fade to dark
static const uint32_t TAP_FLASH_MS         = 900;    // your tap's spark, once round
static const uint32_t TRANSITION_MS        = 300;    // blend into a new mode

static const uint32_t FRAME_MS             = 16;     // ~60fps render tick

static const uint32_t MQTT_RETRY_MS        = 5000;

// ============================================================================
//  STATE
// ============================================================================

enum Mode : uint8_t {
    M_IDLE,       // dark
    M_WAITING,    // her tap: a bloom sinking to a shimmer with fireflies, for TAP_WAIT_MS
    M_FADE,       // fading to dark; anything may interrupt
};

static const char* modeName(Mode m) {
    switch (m) {
        case M_IDLE: return "idle";        case M_WAITING: return "waiting";
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
static uint32_t messageSeed    = 0;     // where her tap's fireflies start and its twinkles fall
static uint32_t tapFlashAt     = 0;     // local "tap registered" spark

// Touch.
static bool     touchRaw = false, touchStable = false, touchHeld = false;
static uint32_t touchChangedAt = 0;

// Message ids.
static uint32_t bootNonce = 0;
static uint32_t eventSeq  = 0;

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

// Every mode change starts from what's actually showing, so nothing snaps.
static void setMode(Mode m) {
    mode = m;
    modeStartedAt = millis();
    memcpy(from, shown, sizeof(shown));
}

static String newEventId() {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s-%08lx-%lu", DEVICE_ID,
             (unsigned long)bootNonce, (unsigned long)++eventSeq);
    return String(buf);
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

// ============================================================================
//  INTERACTIONS
// ============================================================================

// Her tap.
static void receiveTap() {
    bool restart = mode == M_WAITING;
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

static void sendTap() {
    Serial.println("[tap] single -> TAP");
    JsonDocument doc;
    doc["type"] = "TAP";
    publish(doc);
    // Sent or not: the spark means "registered", never "delivered".
    tapFlashAt = millis();
}

// ============================================================================
//  INBOUND
// ============================================================================

static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
    if (topicStatusPeer == topic) {
        bool online = (len == 6 && !memcmp(payload, "online", 6));
        Serial.printf("[mqtt] peer %s\n", online ? "online" : "offline");
        return;
    }
    if (len == 0) return;       // a retained message being cleared

    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) {
        Serial.println("[mqtt] rx malformed json, ignoring");
        return;
    }
    const char* type = doc["type"] | "";

    if (!strcmp(type, "TAP")) {
        Serial.println("[mqtt] rx TAP");
        receiveTap();
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

// --- Her tap, glowing -------------------------------------------------------
// The bloom leaves bright aurora that sinks to a low shimmer, and three
// fireflies circle at their own speeds, each breathing on its own beat, with
// now and then a single LED twinkling white. `rest` is the shimmer it sinks
// to.

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

// End timed modes before drawing.
static void updateTimers(uint32_t now) {
    uint32_t dt = now - modeStartedAt;
    if (mode == M_WAITING && now - messageLitAt >= TAP_WAIT_MS) {
        setMode(M_FADE);
    } else if (mode == M_FADE && dt >= FADE_OUT_MS) {
        setMode(M_IDLE);
    }
}

static Px targetFor(uint8_t i, uint32_t dt) {
    switch (mode) {
    case M_WAITING:   return waiting(i, dt, messageSeed, waitingRest(i, dt));
    default:          return { 0, 0, 0 };       // M_IDLE, and what M_FADE fades to
    }
}

// How far pixel i has blended from what was showing into the current mode.
// Most modes take the whole ring at once. Her tap's bloom sweeps it in
// behind its moving edge instead, so a tap during her glow blooms over it
// rather than dimming it first.
static float entryWeight(uint8_t i, uint32_t dt) {
    switch (mode) {
    case M_WAITING: return min(1.0f, bloomReveal(i, dt) + bloomLight(i, dt));
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

static void serviceTouch() {
    uint32_t now = millis();
    bool raw = digitalRead(PIN_TOUCH) == HIGH;

    if (raw != touchRaw) { touchRaw = raw; touchChangedAt = now; }
    if (now - touchChangedAt >= DEBOUNCE_MS) touchStable = touchRaw;

    if (touchStable && !touchHeld) {                    // press
        touchHeld = true;
    } else if (!touchStable && touchHeld) {             // release
        touchHeld = false;
        sendTap();
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
    } else {
        // rc=-2 is almost always the root CA. rc=4/5 is bad credentials.
        Serial.printf("failed, rc=%d\n", mqtt.state());
    }
}

// Typing into the serial monitor stands in for the touch sensor on the bench.
static void serviceSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "RESET_WIFI") {
        wm.resetSettings(); delay(200); ESP.restart();
    } else if (line == "PING") {            // = a tap
        sendTap();
    } else if (line == "STATUS") {
        Serial.printf("id=%s mode=%s wifi=%d mqtt=%d rssi=%d\n",
                      DEVICE_ID, modeName(mode), WiFi.status() == WL_CONNECTED,
                      mqtt.connected(), WiFi.RSSI());
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

    net.setCACert(MQTT_ROOT_CA);
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMqttMessage);
    mqtt.setBufferSize(512);
    mqtt.setKeepAlive(30);
}

void loop() {
    serviceWifi();
    serviceMqtt();
    serviceTouch();
    serviceSerial();

    uint32_t now = millis();
    if (now - lastFrameAt >= FRAME_MS) {
        lastFrameAt = now;
        renderFrame();
    }
}
