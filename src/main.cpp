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
 *    Tap            A spark runs once round the ring to show the tap
 *                   registered. Nothing is sent to her lamp yet.
 *
 *  PROTOCOL
 *    lamp/<PAIR_ID>/<ID>/signal    this lamp publishes, the other subscribes
 *    lamp/<PAIR_ID>/<ID>/status    "online" / "offline" (last will), retained
 *
 *    Nothing on the signal topic is retained: these are moments, not state.
 *    A lamp that was unplugged simply misses them and boots dark.
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

// Brightest any animation may get, as a fraction of MAX_BRIGHTNESS (which
// stays the USB power cap). Enforced once, per LED, in writeRing().
static const float MAX_LEVEL = 0.70f;

// LEDs respond linearly and eyes don't: without this, fades crawl at the top
// and drop off a cliff at the bottom. 2.6 is what Adafruit's gamma8() uses.
static const float LED_GAMMA = 2.6f;

// ============================================================================
//  TIMING
// ============================================================================

static const uint32_t DEBOUNCE_MS          = 40;

static const uint32_t TAP_FLASH_MS         = 900;    // your tap's spark, once round

static const uint32_t FRAME_MS             = 16;     // ~60fps render tick

static const uint32_t MQTT_RETRY_MS        = 5000;

// ============================================================================
//  STATE
// ============================================================================

// Linear light, before gamma: channel * level, 0..255.
struct Px { float r, g, b; };
static Px px(Rgb c, float level) { return { c.r * level, c.g * level, c.b * level }; }
static Px mix(Px a, Px b, float t) {
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

static Px       shown[NUM_PIXELS];      // what's on the ring right now
static uint32_t tapFlashAt     = 0;     // local "tap registered" spark

// Touch.
static bool     touchRaw = false, touchStable = false, touchHeld = false;
static uint32_t touchChangedAt = 0;

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
//  INTERACTIONS
// ============================================================================

// A tap registered. Nothing goes to her lamp yet.
static void tapped() {
    Serial.println("[tap] tap");
    tapFlashAt = millis();
}

// ============================================================================
//  INBOUND
// ============================================================================

static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
    if (topicStatusPeer == topic) {
        bool online = (len == 6 && !memcmp(payload, "online", 6));
        Serial.printf("[mqtt] peer %s\n", online ? "online" : "offline");
    }
}

// ============================================================================
//  ANIMATION  (called ~60x/sec, never blocks, never allocates)
// ============================================================================
//  Every animation draws each LED separately. Positions round the ring are in
//  fractional pixels, so light glides between LEDs instead of hopping, and
//  most things that travel go the same way: up the pixel numbers.

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

// The moving light itself: a soft bell a few LEDs wide. As it moves, one LED
// brightens while its neighbour dims, so the total stays steady and the light
// glides between LEDs instead of blinking from one to the next.
static float glint(uint8_t i, float head) {
    return bump(apart(i, head), HEAD_PX);
}

// A colour on a looped gradient through `stops`; h in turns, any value.
static Px gradient(const Rgb* stops, uint8_t n, float h, float level) {
    h -= floorf(h);
    float x = h * n;
    uint8_t a = (uint8_t)x % n;
    return mix(px(stops[a], level), px(stops[(a + 1) % n], level), x - floorf(x));
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

static void renderFrame() {
    uint32_t now = millis();
    uint32_t ft = now - tapFlashAt;
    if (tapFlashAt && ft >= TAP_FLASH_MS) tapFlashAt = 0;

    for (uint8_t i = 0; i < NUM_PIXELS; i++) {
        Px out = { 0, 0, 0 };
        // Your own tap: its spark.
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
        tapped();
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
        tapped();
    } else if (line == "STATUS") {
        Serial.printf("id=%s wifi=%d mqtt=%d rssi=%d\n",
                      DEVICE_ID, WiFi.status() == WL_CONNECTED,
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
