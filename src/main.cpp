/*
 * ============================================================================
 *  FRIENDSHIP LAMP
 * ============================================================================
 *
 *  Two lamps. Both run this identical firmware; DEVICE_ID in config.h ("A" or
 *  "B") is the only difference. So far each lamp joins WiFi and the broker
 *  and announces itself; the ring stays dark.
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
 *    "Lamp Setup" portal by itself (first boot, new router). Only the serial
 *    RESET_WIFI erases the saved network.
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

static const uint8_t PIN_PIXELS = 27;   // WS2812B data in

// RING_PIXELS lives in config.h — COUNT THE LEDs on the ring that actually
// arrives. Wrong count = dark pixels or an out-of-range write.
static const uint8_t NUM_PIXELS = RING_PIXELS;

// ============================================================================
//  TIMING
// ============================================================================

static const uint32_t MQTT_RETRY_MS        = 5000;

// ============================================================================
//  STATE
// ============================================================================

// Connectivity.
static uint32_t wifiDownSince = 0;
static uint32_t lastMqttAttempt = 0;

static String topicSignalSelf, topicSignalPeer, topicStatusSelf, topicStatusPeer;

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_PIXELS, NEO_GRB + NEO_KHZ800);
WiFiClientSecure  net;
PubSubClient      mqtt(net);
WiFiManager       wm;

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

// Commands typed into the serial monitor, for checking a lamp on the bench.
static void serviceSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "RESET_WIFI") {
        wm.resetSettings(); delay(200); ESP.restart();
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

    // Dark from power-up: nothing from before the reboot is resurrected.
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
    serviceSerial();
}
