# Friendship Lamp — working rules

ESP32 firmware. Two lamps, one per person. Tap yours, hers glows.

**Full spec is in docs/GUIDE.md.** Read it before any non-trivial change — it
carries the reasoning, the rejected alternatives, and the hardware details.
This file is only the things that will break something if you don't know them.
`README.md` is the short public page; its GIFs in `docs/media/` are recordings
of the build guide's `LampAnim` scenes, so an animation change makes them stale.

## Build

```bash
pio run -t upload        # flash
pio device monitor       # 115200; STATUS, PING, ACK, HOLD [n], RELEASE, TAPS n, SHOW name, LAMP, RESET_WIFI
```

Compiling works here: `~/.local/bin/pio run` (PlatformIO is a uv tool; a
non-login shell may not have `~/.local/bin` on PATH). Behaviour is only
verified by flashing a real board. Say when something compiled but is
otherwise untested rather than implying it works.

## Hard constraints

1. **No `delay()` below `setup()`.** Everything is `millis()`-driven at ~60fps.
   A blocking call stalls MQTT keepalives and trips the 30s watchdog. The only
   exceptions are the WiFi-change blinks and `RESET_WIFI`, which run
   immediately before `ESP.restart()`.

2. **One mode drives the LEDs, and priority decides it.** Shared hold >
   live hold (hers or yours, any look) > secret show > acknowledgment > tap
   message > idle, lamp light, fade.
   - Change modes only with `setMode()`. It captures what's on the ring as the
     blend start, so every transition is smooth; writing `mode` directly
     makes the ring snap.
   - A lower-priority event never interrupts a higher one and is never queued
     (`receiveTap()` and `receiveAck()` check `priorityOf()` first).
   - A higher one replaces a lower one outright. Nothing stashes the old mode
     and nothing resumes it: holds, shows and hold-together always end in
     `M_FADE`. Don't add a "return to what was showing" path. The lamp light
     isn't an exception: while it's on it is the rest state, like dark, so a
     fade ends in `M_LAMP` instead of `M_IDLE`.
   - Holds are decided every loop in `updateHolds()` from two facts both
     lamps can see (my finger, her hold), not from inbound events directly.

3. **After a hold-together ends, the hold that's still down is spent.**
   `endHoldTogether()` marks our own hold `holdSpent` (sends `HOLD_END`, stops
   the heartbeat) and remembers her hold id in `spentPeerHoldId` (ignores its
   remaining heartbeats). Without this the finger still down carries on as a
   solo hold and lights the other lamp straight back up, when both lamps are
   meant to fade.

4. **Read her hold through `peerHoldAlive()`, never the raw `peerHold`.** The
   flag only clears on `HOLD_END`, which may never arrive (power cut, WiFi
   drop). The accessor also requires a `HOLD_START` heartbeat within
   `HOLD_TIMEOUT_MS` and our own MQTT connection, so a lost release can't
   leave her lamp glowing.

5. **Never add `client.setInsecure()`.** TLS validates against a root CA pasted
   into `config.h`. Disabling validation is the shortcut three of the reference
   projects took and it makes the lamp trivially MITM-able.

6. **`src/config.h` is gitignored and holds the broker password.** Never commit
   it, never paste its contents into a chat, never echo it to stdout. Edit
   `config.example.h` when adding a setting.

7. **`RING_PIXELS` must match the physical ring, and the ring runs off USB.**
   Linked Lamp's enclosure fits a 66mm ring; `3d-design/make_lamp.py` builds
   a bigger one sized from `3d-design/settings.py` (85–87mm 24-LED rings).
   Linked Lamp specifies 16 LEDs because the ring draws 5V from the ESP32's
   `VIN` pin (USB-powered, no separate supply). Any 24-LED ring, either size, needs `MAX_BRIGHTNESS` lowered to
   ~120 to keep the same current. Don't raise brightness or add white output
   without checking the USB power budget in GUIDE.md Part 4, "Power".

7a. **The base rim and diffuser wall are frozen** (`LIP_IN = 0.5`,
   `DIFF_WALL = 0.4` in `3d-design/make_lamp.py`). Two bases are already
   printed. The 0.4 mm wall needs the Arachne wall generator in the
   diffuser's Bambu profile; don't "fix" a plate-only print by thickening
   the wall. Reasoning in GUIDE.md, "Bigger lamp for 85–87 mm rings".

8. **Both lamps run identical firmware.** `DEVICE_ID` ("A" or "B") is the only
   difference. Topics derive from it. Never create a second sketch variant.

## Design invariants — don't "simplify" these away

9. **Gesture messages are never retained.** `TAP`, `ACK`, `HOLD_START` and
   `HOLD_END` are moments, not state: a lamp that was unplugged misses them
   and boots dark. On connect each lamp publishes an empty retained message
   to its signal topic, which clears anything the old firmware left there.

10. **Every message has a unique id, and duplicates are dropped**
    (`alreadySeen()`). A hold's repeats share its `hold` id but each has its
    own event id, so they refresh the hold without restarting the glow.

11. **Only her double tap confirms delivery.** Your own lamp's flash means
    "tap recognised", not "tap arrived". Don't add a "sent" or red "failed"
    flash; the spec forbids indicating delivery without the peer's `ACK`.

12. **No coordinator for hold-together.** Each lamp sees its own finger and
    her live hold, and reaches the same answer alone. Don't introduce a
    master, a lock, or a handshake.

13. **The brightness limit is applied once, in `writeRing()`**: the brightest
    channel is capped at `MAX_LEVEL` (70% of `MAX_BRIGHTNESS`, which stays
    the USB power cap). Animations may ask for more; they never get it.

14. **No time-of-day behaviour.** NTP only aligns hold-together and the
    start of secret shows. Quiet hours and similar scheduled behaviour were
    removed on purpose; don't reintroduce them without being asked. The lamp
    light's auto-off (`LAMP_AUTO_OFF_MS`) is a duration from switching on.

14a. **No touch input may erase WiFi credentials or pairing.** The WiFi-change
    hold (plain 10 s hold within 30 s of power-up) only reboots into "Lamp
    Setup" via `portalRequest`; the saved network stays until a new one is
    chosen there. Only the serial `RESET_WIFI` wipes credentials.

14b. **A secret is decided once, by the lamp that recognised it.** It
    publishes `SECRET_GESTURE` with the chosen show (`fx`), a shared start
    time (`at`), a `seed` and the `lead`; the other lamp plays exactly that and
    never re-derives it, so the two can't disagree. Mirror is the one secret
    each lamp works out alone, like hold-together. A show plays locally only
    if it was published.

14c. **Secret holds are ordinary holds with a `look`.** Heartbeats, timeouts,
    spent holds and hold-together all apply unchanged; hold-together shows the
    higher `Look`. A secret hold plays in full on the holder's lamp too, so
    it's fun for the one who made it. Don't give secret holds their own
    modes or messages.

14d. **The secret layer forgets.** Rally turns, handshake steps, Mirror's
    window and the rare-show count live in RAM and reset after
    `SESSION_TIMEOUT_MS` of quiet or on restart. No persistence, dates, streaks
    or statistics, and duplicates never count. The pastels (`PASTELS`,
    `AURORA`) are for the secrets and the lamp light's moving aura; `NEON` is
    for rave alone; the everyday messages stay glacier, cloud and cool white.

14f. **Rave's whole-ring flashes stay under 3 a second.** `RAVE_BEAT_MS`
    must stay above 334 ms (photosensitive-epilepsy limit), and `NEON` has
    no pure red. Make it wilder with beams and single-pixel glitter, which
    may go faster, not by speeding up the kick.

14e. **The build guide draws the animations from a JS port.** The guide's
    Lamp manual (claude.ai/artifact/NWUZS2aGTcL2LgVJx6mHmB, `#using`) runs
    `LampAnim`, a line-for-line copy of the ANIMATION section and mode
    machine. Any change to an animation, its timing or a gesture's mode
    changes goes into that port and its scene scripts too.

## Settled — don't reopen without being asked

15. **Our own firmware, not Linked Lamp's.** We use their 3D models only.
    Reasoning in GUIDE.md Part 5, "Why our own firmware".
16. **EMQX Serverless.** HiveMQ retired its free tier (no new clusters after
    2026-09-30, shutdown 2026-12-31). AWS IoT Core is the fallback if EMQX's
    free tier goes too: cents a month, but device certs and a firmware change.
    Reversible — it's four config fields. GUIDE.md Part 5, "Why EMQX Serverless".
17. **CoronaLamps rejected.** 40+ solder joints, hardcoded WiFi, known
    interrupt crash. GUIDE.md Part 5, "Why not CoronaLamps".
18. **Cuckoo doors parked.** Blocked on enclosure geometry, not software.
    GUIDE.md Part 5, "Parked: the cuckoo doors".

## Style

- Comments explain *why*, not *what*. The ordering traps above are the model.
- Prefer a new `M_*` mode over a boolean flag; `targetFor()` is a switch and
  stays readable that way.
- New tunables go in `config.h` with a comment on how to choose the value, not
  hardcoded in `main.cpp`.
- Runs of 3–5 quick taps are secrets; 6 or more are ignored on purpose, so
  mashing the lamp does nothing. Only mashing *then holding* does something
  (rave), and it's still just a hold. Nothing a finger does may be
  destructive (taps used to reset WiFi).
- A double tap acknowledges her tap whenever one is showing; only with
  nothing to acknowledge does it toggle the lamp light. Keep that order.
