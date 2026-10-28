/*
 * Friendship Lamp — configuration
 *
 * SETUP:
 *   1. Copy this file to src/config.h
 *   2. Fill in your EMQX details below
 *   3. Set DEVICE_ID to "A", flash lamp one
 *   4. Change DEVICE_ID to "B", flash lamp two
 *   5. Everything else stays identical between the two
 *
 * Add src/config.h to .gitignore. It holds your broker password.
 */

#pragma once

// ---------------------------------------------------------------------------
// PER-LAMP — the only thing that differs between the two builds
// ---------------------------------------------------------------------------

// "A" on one lamp, "B" on the other. Nothing else distinguishes them.
#define DEVICE_ID  "B"

// Human name, shown in serial logs only. Purely for your own sanity.
#define OWNER_NAME "Your name"

// ---------------------------------------------------------------------------
// SHARED — identical on both lamps
// ---------------------------------------------------------------------------

// Any random string. Both lamps must match. This namespaces your topics on a
// shared broker, so pick something long enough that nobody collides with it.
// e.g. openssl rand -hex 8
#define PAIR_ID "CHANGEME-2f9a1c4e8b7d3506"

// EMQX Serverless deployment address, no scheme, no port.
// Copy it from the deployment's Overview page in the EMQX console.
#define MQTT_HOST "xxxxxxxx.ala.us-east-1.emqxsl.com"
#define MQTT_PORT 8883          // 8883 = MQTT over TLS. Serverless has no plain 1883; 8084 is WSS.

// Credentials you created under Access Control -> Client Authentication.
// BOTH lamps use the SAME credentials — they're told apart by DEVICE_ID.
#define MQTT_USER "lamp"
#define MQTT_PASS "your-broker-password"

// ---------------------------------------------------------------------------
// BEHAVIOUR
// ---------------------------------------------------------------------------

// Number of LEDs on your NeoPixel ring. COUNT THEM — don't trust the listing.
// 24 for both rings the enclosures are built for (66 mm and 85-87 mm).
// Too low leaves LEDs dark. Too high is harmless (data goes to LEDs that
// don't exist) but wrong.
#define RING_PIXELS 24

// Peak brightness, 0-255. The ring is powered through the ESP32's USB port, so
// this caps current: keep RING_PIXELS * MAX_BRIGHTNESS at or below 2880
// (24 LEDs: 120; 16 LEDs: 180). Animations peak at 70% of this.
#define MAX_BRIGHTNESS 120

// ---------------------------------------------------------------------------
// GESTURES
// ---------------------------------------------------------------------------

// How long her tap glows on your lamp before it fades, in ms. Long enough
// that she can walk into the room and still see it.
#define TAP_WAIT_MS 120000

// ---------------------------------------------------------------------------
// TLS ROOT CA
// ---------------------------------------------------------------------------
/*
 * EMQX Serverless only accepts TLS, so the ESP32 needs the CA certificate
 * EMQX publishes in order to validate the broker.
 *
 * DO NOT skip this and call client.setInsecure(). That disables certificate
 * validation entirely and is the shortcut that makes these projects trivially
 * MITM-able.
 *
 * Get the cert: on the deployment's Overview page in the EMQX console,
 * download the CA certificate (emqxsl-ca.crt). Copy it to the clipboard with
 *
 *   pbcopy < ~/Downloads/emqxsl-ca.crt
 *
 * then paste it over the three placeholder lines between the R"EOF( and
 * )EOF" markers below. Keep its BEGIN and END lines and its line breaks
 * exactly as they come. If the file holds more than one certificate, keep
 * them all.
 *
 * If TLS fails at runtime you'll see "MQTT connect failed, rc=-2" in the serial
 * monitor. Nine times out of ten that's this cert being wrong, missing, or
 * expired — not your credentials.
 */
static const char MQTT_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
PASTE THE EMQX CA CERTIFICATE HERE
-----END CERTIFICATE-----
)EOF";
