# Friendship Lamps

Two WiFi lamps, one per person. Tap yours and hers glows.

This README is self-contained: everything needed to build, use and maintain
the lamps is here.

- **[Part 1 — Build it, step by step](#part-1--build-it-step-by-step)**: start here
- [Part 2 — Using the lamp](#part-2--using-the-lamp)
- [Part 3 — Reference](#part-3--reference): parts, wiring, config, troubleshooting
- [Part 4 — How it works](#part-4--how-it-works)
- [Part 5 — Design decisions](#part-5--design-decisions): why things are the way they are

**Time:** one to two weekends of hands-on work. Printing and shipping are the
slow parts. If there's a deadline, start **six weeks out**, because reprints
and a replacement parts order will eat two of those weeks.

**Golden rule:** change one thing at a time. Hardware has no stack trace, so
testing each stage before the next is the only way to find problems.

---

# Part 1 — Build it, step by step

Tick each box before moving on. Every step says how you know it worked.

## Phase A — Before you buy anything

- [ ] **1. Check her WiFi will work.** Ask now, not after the lamp ships:
  - It must broadcast **2.4 GHz**. The ESP32 can't join 5 GHz-only networks.
  - It must be a normal home network with just a password. A network where
    you log in on a web page (dorms, hotels, some apartment buildings) or with
    a username and password ("enterprise" WiFi) **won't work**.
- [ ] **2. Download the full 3D model set** from
  [MakerWorld](https://makerworld.com/en/models/2886665-linked-lamp-esp32-friendship-lamps)
  or the `3D Models/` folder of
  [Linked-Lamp](https://github.com/aarushmagic/Linked-Lamp). You only need its diffuser:
  save `Neopixel Lamp - 3in Diffuser.step` into `3d-design/linked-lamp-original`.
  The bigger lamp's base and diffuser are built from it; see
  [3D print notes](#3d-print-notes).
- [ ] **3. Learn the four gestures** with her, so you both know what the
  light means. Each lamp shows only soft pale blues and cool white, and is
  dark when nothing is happening.

  | You do | Means | Her lamp |
  |---|---|---|
  | Tap | Thinking of you | A white highlight blooms round the ring, then it breathes pale blue for 10 s |
  | Double tap, while her tap is showing | I got you | Two white heartbeats run round both lamps |
  | Hold | I'm here with you | A blue light glides round the ring for as long as you hold |
  | Both hold at once | We're connected | Both lamps glow pale blue with white waves rolling round, in step |

## Phase B — Order parts

- [ ] **4. Order everything in the [parts list](#parts-list).** Shipping is the
  critical path, so do this first.
  - The parts list gives the exact **Amazon search** for each part and what to
    check on the listing.
  - Everything is listed for **both lamps**, with "buy only if you don't
    have one" and "don't buy" sections so you don't overspend.
  - Each lamp is powered like Linked Lamp's: a USB cable plugged straight
    into the ESP32, from a phone charger. The ring takes its power from the
    board.
  - Budget is roughly **$50–80** for both lamps, plus any tools you don't
    have. That's an estimate; prices vary a lot by store.

## Phase C — Software setup (while parts ship)

> **How to run the commands in this guide.** Each code box is something to
> type into the **Terminal** app on your Mac.
> 1. Open Terminal: press ⌘ Space, type `Terminal`, press Return.
> 2. Copy a box, click in the Terminal window, press ⌘ V, then Return.
> 3. Wait for the line ending in `%` to come back before running the next
>    one. That means the command has finished.
> 4. To stop something that keeps running, like the serial monitor, press
>    Control C.

- [ ] **5. Install PlatformIO.** It builds the firmware and copies it onto the
  board. It's installed with **uv**, which keeps it in its own space so it
  can't clash with anything else on your Mac.
  1. Open Terminal: press ⌘ Space, type `Terminal`, and press Return.
  2. Copy this, click in the Terminal window, press ⌘ V to paste, then press
     Return. Wait until the line ending in `%` comes back; it can take a
     minute.
     ```bash
     brew install uv
     ```
  3. Copy this, paste it with ⌘ V, then press Return. Wait until it prints
     `Installed 3 executables: pio, piodebuggdb, platformio`.
     ```bash
     uv tool install --python 3.13 --managed-python platformio
     ```
  4. Copy this, paste it with ⌘ V, then press Return:
     ```bash
     uv tool update-shell
     ```
  5. Quit Terminal completely: press ⌘ Q.
  6. Open Terminal again: press ⌘ Space, type `Terminal`, and press Return.
  7. Copy this, paste it with ⌘ V, then press Return:
     ```bash
     pio --version
     ```

  ✅ **It worked if** it prints `PlatformIO Core, version 6.` followed by more
  numbers.
  ❌ **`command not found: brew`:** install Homebrew from
  [brew.sh](https://brew.sh) first, then start this step again.
  **`command not found: pio`:** you're in an old Terminal window. Press ⌘ Q,
  open Terminal again, and repeat the last action.
  Don't use `pip install platformio`: Homebrew's Python refuses it with
  `externally-managed-environment`, on purpose. To update PlatformIO later,
  run `uv tool upgrade platformio`.
- [ ] **6. Create the broker** on EMQX Serverless (free). The broker is the
  online relay the two lamps talk through. Open a note (the Notes app is
  fine) to collect three things as you go: the **address**, a **username** and
  a **password**. You'll also download a certificate file.
  1. In your browser, go to
     [EMQX Serverless](https://www.emqx.com/en/cloud/serverless-mqtt) and sign
     up for a free account. Confirm your email if it asks.
  2. Log in to the console. If it asks you to create a project, give it any
     name and create it.
  3. Click **New Deployment**.
  4. Choose **Serverless**. If it asks for a region, pick the one nearest to
     you both.
  5. Find **Spend Limit** and make sure it says **0**. That keeps you on the
     free monthly allowance: if it's ever used up, the broker pauses instead
     of billing you.
  6. Type the deployment name `friendship-lamps`, accept the terms, and click
     **Deploy**.
  7. Wait a minute or two until the deployment shows as running, then click it
     to open it.
  8. In the deployment's menu, click **Access Control**, then
     **Client Authentication**.
  9. Click **Add**. Type a username, such as `lamp`, and a password, and save.
     Copy both into your note. Both lamps share this one login.
  10. In the deployment's menu, click **Overview**.
  11. Find the **address**. It ends in `.emqxsl.com`. Copy it into your note.
      The port is **8883**, which the lamp already uses.
  12. Find the **CA certificate** and click to download it. It saves to your
      Downloads folder as `emqxsl-ca.crt`.

  ✅ **It worked if** your note has the address, username and password, and
  `emqxsl-ca.crt` is in your Downloads folder.
- [ ] **7. Create your config file.** It holds everything specific to your
  lamps, including the broker password, so it's kept out of git.
  1. Copy this, click in the Terminal window, press ⌘ V to paste, then press
     Return. Nothing prints. That's normal: you're now in the project folder.
     ```bash
     cd ~/Projects/lamp_proj
     ```
  2. Copy this, paste it with ⌘ V, then press Return. Nothing prints; that's
     normal too.
     ```bash
     cp src/config.example.h src/config.h
     ```
  3. Copy this, paste it with ⌘ V, then press Return. It prints 16 letters and
     numbers, such as `4f1c9a07be52d318`. That's your pairing code. Select it
     with the mouse, press ⌘ C, and paste it into your note.
     ```bash
     openssl rand -hex 8
     ```
  4. Copy this, paste it with ⌘ V, then press Return. VS Code opens with the
     file. Don't use TextEdit: it turns `"` into curly quotes, which breaks the
     file.
     ```bash
     open -a "Visual Studio Code" src/config.h
     ```
  5. In VS Code, change each line below. Select the old text **between the
     quotes** with the mouse, then type or paste the new text. Keep the quotes
     themselves.

     | Line starting | Put between the quotes |
     |---|---|
     | `#define DEVICE_ID` | Leave it as `A` |
     | `#define OWNER_NAME` | Your name. It only appears in the monitor. |
     | `#define PAIR_ID` | The pairing code from your note |
     | `#define MQTT_HOST` | The address from your note, with nothing in front and no `:8883` after it |
     | `#define MQTT_USER` | The username from your note |
     | `#define MQTT_PASS` | The password from your note |

     Leave everything else as it is. `RING_PIXELS` gets checked in step 13.
  6. Click the Terminal window. Copy this, paste it with ⌘ V, then press
     Return. Nothing prints. The certificate is now on your clipboard.
     ```bash
     pbcopy < ~/Downloads/emqxsl-ca.crt
     ```
  7. Click the VS Code window and press ⌘ ↓ to jump to the bottom of the file.
  8. Drag with the mouse to select these three lines, from the first dash to
     the last:
     ```text
     -----BEGIN CERTIFICATE-----
     PASTE THE EMQX CA CERTIFICATE HERE
     -----END CERTIFICATE-----
     ```
  9. Press ⌘ V to paste the certificate over them. The `R"EOF(` line above
     and the `)EOF";` line below stay where they are.
  10. Press ⌘ S to save.
  11. Click the Terminal window. Copy this, paste it with ⌘ V, then press
      Return. If everything is filled in, nothing prints.
      ```bash
      grep -nE 'CHANGEME|xxxxxxxx|your-broker-password|PASTE THE|“|”' src/config.h
      ```
  12. Copy this, paste it with ⌘ V, then press Return. It prints a number.
      ```bash
      grep -c "BEGIN CERTIFICATE" src/config.h
      ```

  ✅ **It worked if** the first check prints nothing and the second prints
  `1` or more.
  ❌ **The first check printed a line:** that setting still has its
  placeholder or a curly quote. Fix it in VS Code, save, and run the check
  again. **The second printed `0`:** the certificate didn't paste; redo the
  certificate actions. **`no such file or directory`:** the certificate
  downloaded under another name. Look in Downloads and use that name instead.

## Phase D — Two bare boards talking (the milestone that matters)

No soldering yet. Just two ESP32s on USB.

- [ ] **8. Flash board A.** `config.h` says `DEVICE_ID "A"`, so whatever you
  flash now becomes lamp A.
  1. Plug board A into your Mac with the data cable, through the adapter if
     your Mac only has USB-C.
  2. Copy this, click in the Terminal window, press ⌘ V to paste, then press
     Return. Look for a line starting with `/dev/cu.usbserial`,
     `/dev/cu.wchusbserial` or `/dev/cu.SLAB_USBtoUART`. Ignore Bluetooth
     entries.
     ```bash
     pio device list
     ```
  3. Copy this, paste it with ⌘ V, then press Return:
     ```bash
     cd ~/Projects/lamp_proj
     ```
  4. Copy this, paste it with ⌘ V, then press Return. This builds the
     firmware, copies it onto the board and opens the serial monitor. Lots of
     text scrolls past. The first time takes several minutes.
     ```bash
     pio run -t upload -t monitor
     ```
  5. Wait until you see `--- Terminal on /dev/cu.usbserial…`. The monitor is
     now open and shows what the board prints. Leave this window open for
     step 9.
  6. If `=== Friendship Lamp A (John) ===` doesn't appear below it, press the
     small **EN** button beside the board's USB socket. Not BOOT; some boards
     label EN as **RST**. EN only restarts the board: it restarts by itself
     after every upload, so you need EN only if the monitor opened too late
     to catch the start.

  ✅ **It worked if** the upload ends with `Hard resetting via RTS pin...`,
  then the monitor prints `=== Friendship Lamp A (John) ===` with your
  `OWNER_NAME`, and a few lines later `*wm:StartAP with SSID: Lamp Setup`.
  **Reflashing a board that already joined your WiFi:** you see
  `[wifi] connected, ip=...` instead of Lamp Setup. An upload keeps the
  saved WiFi, so skip step 9.
  ❌ **No `/dev/cu.usbserial…` entry:** the cable is probably charge-only, or
  the board needs a driver; see [Troubleshooting](#troubleshooting).
  **Upload stuck at `Connecting...`:** hold the board's **BOOT** button until
  percentages appear, then let go. **`Unable to verify flash chip
  connection`:** see [Troubleshooting](#troubleshooting). **Errors that
  mention `config.h`:** a missing quote; compare that line with
  `src/config.example.h`.
- [ ] **9. Connect it to WiFi.** The board doesn't know your WiFi yet, so it
  has opened its own network called **"Lamp Setup"**. Keep the monitor from
  step 8 open.
  1. On your phone, open **Settings → Wi-Fi** and tap **Lamp Setup** to join
     it.
  2. A setup page opens by itself. If it doesn't, open the phone's browser,
     type `192.168.4.1` in the address bar and go.
  3. Tap **Configure WiFi**.
  4. Tap your home network in the list, type its password, and tap **Save**.
  5. Your phone leaves Lamp Setup by itself. Look at the Terminal window.

  You have 5 minutes. If you miss it, the board reboots and opens Lamp Setup
  again.
  ✅ **It worked if** the monitor shows `[wifi] connected, ip=...`, then
  `[mqtt] connecting as lamp-A-... connected`.
  ❌ **`rc=-2`:** the address or the certificate is wrong; recheck
  `MQTT_HOST` and redo the certificate actions in step 7. **`rc=4` or
  `rc=5`:** the username or password is wrong. **Your network isn't in the
  list:** it's probably 5 GHz only; the ESP32 needs 2.4 GHz.
- [ ] **10. Flash board B.** Same firmware, with one setting changed. **Only
  one board may be plugged in while flashing**: with two connected, the
  upload goes to whichever it finds first, and you could overwrite A.
  1. Click the Terminal window and press Control C to stop the monitor. The
     line ending in `%` comes back.
  2. Unplug board A.
  3. Click the VS Code window with `config.h` and change two lines:
     `DEVICE_ID` to `"B"`, and `OWNER_NAME` to her name.
  4. Press ⌘ S to save.
  5. Plug in board B.
  6. Click the Terminal window. Copy this, paste it with ⌘ V, then press
     Return:
     ```bash
     cd ~/Projects/lamp_proj
     ```
  7. Copy this, paste it with ⌘ V, then press Return. Wait until you see
     `=== Friendship Lamp B (...) ===`. If it doesn't appear, press board B's
     **EN** button.
     ```bash
     pio run -t upload -t monitor
     ```
  8. If the monitor shows `*wm:StartAP with SSID: Lamp Setup`, connect board
     B to WiFi: do step 9 again on your phone. If it shows
     `[wifi] connected, ip=...`, B already knows your WiFi; skip this.
  9. Click the VS Code window. Change the two lines back to `"A"` and your
     name, and press ⌘ S. Then the next time you flash A, it
     can't accidentally become a second B.
  10. Click the Terminal window and press Control C to stop the monitor.

  ✅ **It worked if** the monitor printed `=== Friendship Lamp B (...) ===`
  with her name, then `[mqtt] ... connected`.
  ❌ **It printed `Lamp A`:** `config.h` wasn't saved before flashing. Save
  it and run the flash command again.
- [ ] **11. Ping between them.** Both boards are plugged into the Mac, each
  with its own monitor tab. You send `PING` from one tab and watch it arrive
  in the other.

  **Find the two port names**
  1. Plug both boards into the Mac.
  2. Click the Terminal window. If it's still showing a monitor, press
     Control C. Wait for the line ending in `%`.
  3. Copy this, click in the Terminal window, press ⌘ V to paste, then press
     Return:
     ```bash
     pio device list
     ```
  4. Find the two lines that start with `/dev/cu.usbserial`. Those are your
     port names, such as `/dev/cu.usbserial-10` and `/dev/cu.usbserial-110`.
     Write both down.

  **Open a monitor for each board**
  5. Copy this, paste it with ⌘ V, then press Return:
     ```bash
     cd ~/Projects/lamp_proj
     ```
  6. Copy this and paste it with ⌘ V, but **don't press Return yet**:
     ```bash
     pio device monitor --port /dev/cu.usbserial-10
     ```
  7. If your first port name ends differently, press Delete to erase the
     number at the end and type yours.
  8. Press Return. You see
     `--- Terminal on /dev/cu.usbserial-10 | 115200 8-N-1`. This tab now
     shows one board.
  9. Press ⌘ T. A new tab opens.
  10. Copy this, paste it with ⌘ V, then press Return:
      ```bash
      cd ~/Projects/lamp_proj
      ```
  11. Copy this and paste it with ⌘ V, but **don't press Return yet**:
      ```bash
      pio device monitor --port /dev/cu.usbserial-110
      ```
  12. If your second port name ends differently, press Delete to erase the
      number at the end and type yours.
  13. Press Return. You see
      `--- Terminal on /dev/cu.usbserial-110 | 115200 8-N-1`.

  **Find which tab is board A**
  14. Press the small **EN** button beside board A's USB socket (not BOOT).
  15. Click each tab in turn at the top of the Terminal window. The one
      showing `=== Friendship Lamp A (John) ===` is A's tab. The other is
      B's.
  16. Stay on A's tab and wait until a line ends in `connected`:
      `[mqtt] connecting as lamp-A-... connected`. That takes about 10
      seconds.

  **Send the pings**
  17. Copy this, click inside A's tab, press ⌘ V to paste, then press Return.
      A's tab shows `[tap] single -> TAP`, then a line starting `[mqtt] tx`
      and ending `-> ok`.
      ```text
      PING
      ```
  18. Click B's tab and look for `[lamp] lighting for 10s (tap from peer)`.
  19. Click inside B's tab, press ⌘ V to paste `PING` again, then press
      Return.
  20. Click A's tab and look for `[lamp] lighting for 10s (tap from peer)`.
  21. Leave both tabs open for step 12.

  ✅ **It worked if** each board's tab prints
  `[lamp] lighting for 10s (tap from peer)` when the other board pings.
  ❌ **The sending tab says `offline, TAP not sent`:** you pinged before it
  reconnected. Wait for the line ending in `connected` and try again.
  **Nothing happens when you paste:** click inside the tab first, then paste.

  Paste `PING` rather than typing it: the board only waits a second between
  letters, and it must be in capitals. Both boards must be connected at the
  same time: taps aren't stored on the broker, so a lamp that's unplugged when
  you tap simply misses it.
- [ ] **12. Reboot your router** while the boards are running. This checks
  they recover from a real outage on their own.
  1. Leave both monitor tabs from step 11 open.
  2. Unplug your router's power cable.
  3. Wait 30 seconds, then plug it back in.
  4. Wait 2–3 minutes for the router's lights to settle. Meanwhile each tab
     shows `[wifi] link down, reconnecting`, and later a line ending in
     `connected`.
  5. Click inside A's tab, press ⌘ V to paste `PING` (it's still on your
     clipboard), then press Return.
  6. Click B's tab and look for `[lamp] lighting for ...`.

  ✅ **It worked if** both tabs reconnected by themselves, and B printed
  `[lamp] lighting for ...` after the router came back.
  **Can't take the router down right now?** Leave this step unticked and
  carry on. Step 35 unplugs the router during the 24-hour test, so you can do
  both together at a time that suits you.

**This is the whole product.** Everything after this is packaging.

## Phase E — First hardware (lamp A only)

Wire **board A only**. Board B stays bare until Phase F and stands in for her
lamp: pasting `PING` into B's monitor tab is the same as her tapping. The
[visual wiring guide](https://claude.ai/artifact/NWUZS2aGTcL2LgVJx6mHmB#wiring)
has a picture of every connection.

> **Power down before you touch any wire.** Connecting or moving a wire while
> a board is plugged in can damage it. Do this now, and before every wiring
> step in this phase:
> 1. Click A's monitor tab and press Control C. The line ending in `%` comes
>    back. Keep the tab open; you'll reuse it.
> 2. Unplug the USB cable from board A. Leave the other end in the same
>    socket on your Mac, so A's port name stays the same.
> 3. Leave board B plugged in, with its monitor tab running.
>
> **Powering back up after wiring:** keep your fingers off the touch sensor,
> plug the cable back into board A, click A's tab, press ↑ to bring back the
> monitor command, and press Return.

- [ ] **13. Count the LEDs on your ring.**
  1. Look at the front of the LED ring and count the small square LEDs.
     Count twice to be sure.
  2. **24 LEDs:** nothing to change; `config.h` is already set up for 24.
     This step is done.
  3. **16 LEDs:** click the VS Code window with `config.h`. Change
     `#define RING_PIXELS 24` to `16`, and `#define MAX_BRIGHTNESS 120` to
     `180` (see [why](#why-these-choices)). Press ⌘ S to save.
  4. **16 LEDs, continued:** reflash both boards, one at a time with only
     that board plugged in: board A as in step 8, then board B as in step 10,
     which includes switching `config.h` to B and back.
- [ ] **14. Learn to solder.** Make 30 practice joints on wire offcuts and
  throw them away. Read the [soldering tips](#soldering-tips) first. If your
  touch sensor has no pins, step 15 has you solder three wires into it: good
  first real joints after your practice ones.
- [ ] **15. Wire the touch sensor only.** You'll solder a wire into each of
  the touch sensor's three holes, then push the other ends onto board A.
  Board A stays unplugged until the last part. If three metal pins already
  stick out of your sensor, skip parts B and C and push the wires' sockets
  straight onto those pins. The
  [online guide](https://claude.ai/artifact/NWUZS2aGTcL2LgVJx6mHmB) has a
  picture for each part.

  **A. Get ready**
  1. Lay out: the touch sensor, three jumper wires with a socket on at least
     one end (male-female or female-female) in three
     different colours (ideally orange, black and blue), the soldering iron
     with its stand and damp sponge, solder, wire strippers, and the helping
     hands.
  2. Pick up the sensor. It's a small square board. The side with a tiny
     black chip on it is the **back**. The other side, mostly plain, is the
     **front**: that's what you'll touch.
  3. Find the three small holes in a row along one edge. Next to each is a
     printed label, on the front or the back: `VCC` (or `VIN`), `GND`, and
     `I/O` (or `IO`, `OUT`, `SIG`). The order varies between makers, so read
     yours.
  4. Decide the colours: **orange** goes in `VCC`, **black** in `GND`,
     **blue** in `I/O`. These match the Wiring guide's pictures. If
     you've already cut a different colour, use it and write down which
     colour you're using for which.
  5. Switch the iron on and set it to about **330 °C** for leaded solder or
     **360 °C** for lead-free (it says which on the solder's label). Wait a
     minute for it to heat.
  6. Wipe the iron's tip on the damp sponge, then touch a little solder to the
     tip so it's coated and shiny.

  **B. Make three leads**
  7. Take the orange wire. Cut off one end about 2 cm from it (the **pin end**
     on a male-female wire; either end on a female-female one),
     using the cutter on your wire strippers. Keep the long piece with the
     socket; throw the short piece away.
  8. Strip 3 mm of insulation off the cut end: put the wire in the stripper's
     **24** notch, 3 mm from the end, squeeze, and pull the insulation off.
     If it won't come off, use the **26** notch.
  9. Twist the bare strands tightly between finger and thumb so none stick
     out.
  10. Tin the strands: hold the iron's tip against them for 2 seconds, then
      touch the solder to the strands (not to the iron) until it soaks in. The
      strands turn silver and stiff. Put the iron back in its stand.
  11. Do the same for the black wire and the blue wire (actions 7–10).

  **C. Solder the leads into the sensor**
  12. Hold the sensor back side up. Push the orange lead's silver tip into the
      `VCC` hole from the back, until about 2 mm pokes out of the front.
  13. Bend the orange wire gently along the back of the sensor so it holds
      itself in place.
  14. Clip the sensor in the helping hands by the edge *opposite* the holes,
      **front side up**, so the orange tip points up at you.
  15. Touch the iron's tip to both the orange wire's tip and the copper ring
      around the hole at the same time. Hold it there for 2 seconds.
  16. Still holding the iron there, touch the solder to where the wire meets
      the copper ring (not to the iron) and let a little melt in, until it
      flows round into a small shiny cone. About 1–2 seconds.
  17. Take the solder away first, then the iron. Don't move anything for 3
      seconds while it sets.
  18. Unclip the sensor. Repeat actions 12–17 for the black lead in `GND`,
      then the blue lead in `I/O`.
  19. Look at the three joints on the front. Each should be a small smooth
      cone hugging its wire and copper ring, and **no solder may join two
      neighbouring holes**. If two touch, reheat between them and drag the
      extra away with the tip.
  20. Give each wire a gentle tug. It mustn't move.
  21. Leave the two tiny pads marked **A** and **B** on the back unsoldered.
      Joining them changes how the sensor behaves.
  22. Switch the iron off and leave it in its stand to cool.

  **D. Connect the sensor to board A**
  23. Check board A's USB cable is unplugged (the box at the top of this
      phase).
  24. Hold board A labels-up, with its USB socket at the top. Its pins stick
      out underneath; the label for each pin is printed beside it on top.
  25. Find `3V3`: the first pin below the USB socket, on one side. The first
      pin on the *other* side is `VIN`. Don't mix them up.
  26. Push the **orange** wire's socket onto the `3V3` pin from underneath, all
      the way on.
  27. Push the **black** wire's socket onto the `GND` pin right next to it
      (2nd pin from the USB socket, same side).
  28. Count down that same side from the USB socket: `3V3`, `GND`, `D15`,
      `D2`, `D4`. Push the **blue** wire's socket onto `D4`, the 5th pin.
      Check the printed label says `D4`.
  29. Check: orange on `3V3`, black on `GND`, blue on `D4`, and nothing on the
      `VIN` side.
  30. Lay board A down so the wires aren't pulling, with the sensor front
      side up and not touching anything metal.

  **E. Power up and test**
  31. Keep your fingers off the sensor: it calibrates itself the moment it
      gets power.
  32. Plug the USB cable back into board A.
  33. Click A's monitor tab, press ↑ to bring back the monitor command, and
      press Return.
  34. Wait until A's tab shows a line ending in `connected`.
  35. Tap the front of the sensor once with your fingertip.
  36. Look at A's tab, then click B's tab.
  37. Stop here. The ring is step 16.

  ✅ **It worked if** A's tab prints `[tap] single -> TAP` and a line ending
  `-> ok`, and B's tab prints `[lamp] lighting for 10s (tap from peer)`.
  ❌ **Nothing prints when you tap:** do the power-down steps, check the wire
  colours against part D (especially blue on `D4`), and look at each joint:
  the solder must hug both the wire and the copper ring. Reheat any that look
  lumpy or loose. Then power up again without touching the sensor. **Taps
  fire on their own:** unplug board A and plug it back in without touching
  the sensor. **A's tab stays blank after Return:** the cable went into a
  different socket on your Mac, so the port name changed; run
  `pio device list` and use the new name.
- [ ] **16. Wire the ring**, following the [wiring](#wiring). The ring has
  bare pads, so make three leads: cut two female-female jumper wires in half,
  strip 3 mm off each cut end, and solder one lead to each of the ring's
  **5V, GND and DIN** pads. These are the project's only essential solder
  joints. Push the female ends onto the ESP32: `5V` → `VIN`, `GND` → the `GND`
  pin next to `VIN`, `DIN` → `D27`. Never use `3V3` for the ring, and use the
  **DIN** (data in) pad, not DOUT.
  ✅ **It worked if** pasting `PING` into B's monitor tab makes a white
  highlight bloom round A's ring, which then breathes pale blue and fades out
  about 13 s later. The ring stays dark
  while the lamp starts up; that's normal.
- [ ] **17. Put it all together.** Tap board A's sensor, then paste `PING`
  into board B's monitor tab. Then hold A's sensor for a few seconds.
  ✅ **It worked if** the tap makes A flash pale blue and B's tab print
  `[lamp] lighting for 10s (tap from peer)`; B's `PING` makes A's ring bloom
  and glow pale blue for 10 s; and while you hold A, its ring glows softly and B's tab
  prints `[mqtt] rx HOLD_START`.

## Phase F — Print and assemble

Your brother prints the parts on his Bambu P2S from his phone. You publish
them on MakerWorld once (step 18), then he prints them (step 19). Bambu's
button names may differ slightly from these.

- [ ] **18. Publish the lamp on MakerWorld (once).** His phone can only print
  parts that are on MakerWorld with print profiles: saved Bambu Studio
  projects. About an hour.

  **A. Install Bambu Studio**
  1. Copy this, click in the Terminal window, press ⌘ V to paste, then press
     Return. Wait for the line ending in `%`; it can take a few minutes.
     ```bash
     brew install --cask bambu-studio
     ```
  2. Open Bambu Studio: press ⌘ Space, type `Bambu Studio`, and press Return.
  3. In the setup screens, sign in with a Bambu account. The same login works
     on MakerWorld; create one if you don't have one.
  4. When it asks for a printer, choose **Bambu Lab P2S** with the
     **0.4 mm** nozzle.

  **B. Make the three print profiles.** The files are in
  `3d-prints › 1 Print these`. Do actions 5–9 once for each row:

  | Profile | Drag onto the plate |
  |---|---|
  | `Fit test` | `1 Fit test - base rim.stl` |
  | `Diffuser` | `2 Diffuser - white PLA - print 2.stl` |
  | `Base` | `3 Base - dark PLA - print 2.stl` |

  5. In Bambu Studio, click **File → New Project**.
  6. Drag the file for that row from Finder onto the plate.
  7. In the filament list on the left, choose **Bambu PLA Basic**. Leave the
     other print settings alone: Bambu's defaults are already 0.2 mm layers,
     2 walls, 15% infill and no supports.

     **Diffuser row only:** on the left, find the **Process** row and click
     the **Advanced** switch beside it so it turns on. Click the **Quality**
     tab, scroll to **Wall generator**, and change **Classic** to
     **Arachne**. Can't find it? Click the magnifier beside the
     `0.20mm Standard @BBL P2S` box, type `wall generator`, and click the
     result. The diffuser's walls are one 0.4 mm line, and with Classic only
     its flat top plate prints.
  8. Click **Slice plate** (top right). Check it finishes without errors, and
     that the preview shows the whole part, not just a flat plate. For the
     diffuser, the top of the slider on the right should read about
     **509** and **101.80**, and Total time about **1 hour**. If it reads
     **5** and **1.00** (about 25 minutes), Wall generator is still on
     Classic.
  9. Click **File → Save Project As**. Open the folder
     `3d-prints › 2 MakerWorld upload kit › print profiles - save Bambu Studio files here`,
     name the file after the profile, and save.

  **C. Upload to MakerWorld**
  10. Open `3d-prints › 2 MakerWorld upload kit › Listing text - copy into
      MakerWorld.md`. It has the title, description, license and remix link
      to paste.
  11. Go to [makerworld.com](https://makerworld.com), sign in, and click
      **Upload**.
  12. Add the three `.stl` files from `1 Print these`, and the four pictures
      from `2 MakerWorld upload kit › pictures`.
  13. Paste the title and description. Choose license **CC BY-SA 4.0**, mark
      the model as a **Remix**, and paste the Linked Lamp link from the
      listing text.
  14. Publish. Then, on the model's page, click **+Add** under Print Profile
      and upload each of your three `.3mf` files, named Fit test, Diffuser
      and Base.
  15. Copy the model's link and text it to your brother.

  ✅ **It worked if** the listing is live with three print profiles: Fit
  test, Diffuser and Base.
- [ ] **19. Print the parts.** Your brother prints everything on his P2S from
  the Bambu Handy app. Bambu's cloud prepares each print for his printer, so
  he needs no computer and no files.

  **A. How he prints a profile**
  1. He opens the link you texted on his phone. It opens in **Bambu Handy**,
     signed in to the account his P2S uses.
  2. He chooses the print profile, taps **Print**, and picks his P2S.
  3. He picks the spool: white PLA for diffusers, dark PLA for bases, any
     colour for the fit test.
  4. He taps **Send**.

  **B. Fit test first (about 40 minutes)**
  5. He prints the **Fit test** profile: the top 5 mm of the base.
  6. If you can get one of your LED rings to the piece, it should drop into
     the round seat.

  **C. One diffuser, then the rest**
  7. He prints the **Diffuser** profile once, in white PLA.
  8. Turn the diffuser open end down and slide it over the fit-test piece's
     rim. It should slide on snugly without forcing.
  9. If it fits, he prints the **Diffuser** profile a second time.
  10. He prints the **Base** profile twice, in dark PLA.
  11. Hold a diffuser over a lit ring to check the glow: paste `PING` into
      B's monitor tab to light A.

  ✅ **It worked if** two diffusers and two bases are printed, the first
  diffuser slid over the fit-test rim, and the ring dropped into its seat.
  ❌ **Diffuser too tight or loose on the rim, or the ring doesn't fit:** tell Claude what
  happened. The sizes are in `3d-design/settings.py`;
  `3d-design/HOW TO CHANGE THE LAMP.txt` explains how to rebuild the files.
- [ ] **20. Assemble lamp 1 (A) only.** The wiring from Phase E goes in as it
  is. Fit everything first, before taping or gluing anything.
  1. Unplug board A's USB cable (the power-down box at the top of Phase E).
  2. Place the ESP32 upside down on the four posts inside the base, so the
     small pin on each post goes through one of its mounting holes and its
     USB-C socket faces the small slot near the bottom of that flat side.
  3. Check the USB-C cable plugs into the socket through that slot. This
     cable powers the lamp, and you'll plug your computer into the same
     socket to reflash.
  4. On the opposite flat side, slide the touch sensor down into the slot
     inside the wall, touch side facing the wall. The round dimple on the
     outside marks where to touch.
  5. Seat the LED ring in the round seat at the top, LEDs facing up. Turn it
     so the pads sit over the open middle or one of the two notches, so the
     wires drop down without being pinched.
  6. Tape or glue the ring's three wires down near the pads, so moving them
     can't flex and crack the joints. Leave the jumper sockets pushed onto
     the ESP32's pins.
  7. Slide the diffuser down over the base's rim.
  8. Plug in the USB cable, open A's monitor tab, and tap the dimple. Then
     paste `PING` into B's tab.

  ✅ **It worked if** tapping makes B's tab print `[lamp] lighting for ...`,
  and B's `PING` lights A's ring through the diffuser, exactly as it did on
  the bench.
- [ ] **21. Wire and assemble lamp 2 (B)** the same way: steps 15–17 on board
  B, then step 20. Don't build both in parallel, or you'll make the same
  mistake twice.

## Phase G — Test everything

Do these with both lamps side by side: lamp A on the left, B on the right.
Keep both monitor tabs open; each test says what the lamps should do, and the
tabs show what each lamp heard.

**Tap: "Thinking of you"**
- [ ] **22. Tap.** Tap A once. A flashes pale blue briefly. On B a white
  highlight appears at one spot and spreads both ways round the ring, then B
  breathes pale blue and fades out about 13 s later. Repeat from B to A.
- [ ] **23. Taps restart, never stack.** Tap A, then tap it again 5 s later.
  B blooms again over its glow without going dark, and fades out about 13 s
  after the **second** tap.

**Double tap: "I got you"**
- [ ] **24. Acknowledge a tap.** Tap A. While B is glowing, double-tap B.
  Two white heartbeats with pale-blue trails run round both lamps, then both
  fade out over 3 s.
- [ ] **25. Double tap with nothing showing: lamp light.** With both lamps
  dark, double-tap A. A white point bursts round A's ring and opens into a
  slowly moving aura of soft colour; B stays dark. A's tab prints
  `[tap] double -> lamp light on`. Double-tap A again: the light falls back
  into a point and goes out.

**Hold: "I'm here with you"**
- [ ] **26. Hold.** Hold A for a few seconds. A glows softly, and a blue
  light glides round B's ring the whole time. Let go: both fade out over 3 s.
- [ ] **27. Power cut mid-hold.** Hold A, and while B is glowing, unplug A.
  B fades out by itself within about 6 s: 3 s to notice, 3 s to fade.

**Hold together: "We're connected"**
- [ ] **28. Hold together.** Hold A, then hold B as well. Both lamps glow pale
  blue with white waves rolling slowly round them, in step.
- [ ] **29. One lets go.** While holding together, let go of A but keep
  holding B. Both lamps fade out over 3 s, and neither lights again while B
  is still held.

**Gestures overlapping**
- [ ] **30. A hold beats a tap.** Tap A so B glows, then hold B. A switches to
  B's hold glow. Let go of B: both fade out, and B's tap glow doesn't come
  back.
- [ ] **31. A tap during a hold is ignored.** Hold A, so B glows. Tap B. A
  keeps showing the hold as it was, and no tap glow appears on A after you
  let go.
- [ ] **32. A double tap during a hold is ignored.** Tap A so B glows, then
  hold A. While B shows the hold, double-tap B. No white heartbeats.
- [ ] **33. A hold during a fade.** Tap A, and when B's glow starts fading
  (about 10 s later), hold A. B moves smoothly from the fade into the hold
  glow without going dark first.
- [ ] **34. Rapid taps are safe.** Tap A eight times or more, as fast as you can,
  and lift your finger after the last one. Nothing happens on either lamp and A's WiFi stays as it was; A's tab prints
  `taps ignored`.

**Endurance**
- [ ] **35. 24-hour soak test.** Leave both lamps running for a day. Unplug the
  router mid-test and confirm both recover by themselves. **Don't skip this.**

## Phase H — Hand it over

- [ ] **36.** Before giving her the lamp, check board B starts up as hers:
  its monitor prints `=== Friendship Lamp B (...) ===` with her name (step 10).
- [ ] **37.** Give her three things: the **lamp**, its **USB-A-to-C cable**
  and a **USB-A wall charger** (5 V, 2 A). She doesn't need the adapter; that's
  only for your computer. At her place: plug it in and wait about 20 s. It
  can't find your WiFi, so it opens **"Lamp Setup"**; the ring stays dark
  meanwhile. Join that from her phone and choose her WiFi. If the page doesn't
  pop up by itself, open a browser and go to `192.168.4.1`.
  **Send her the step-by-step page,**
  [Friendship Lamp WiFi Setup](https://claude.ai/artifact/JTsLRpTzXAAjtCcTkxU82Y): each phone screen
  she'll see, fixes, and how to change WiFi later. Share it first (its
  **Share** menu, anyone with the link), and ask her to open it before she
  starts: while her phone is on Lamp Setup it has no internet.
- [ ] **38.** Tell her how to change WiFi:
  - **Router replaced or password changed:** unplug the lamp and plug it back
    in. It can't find the old network, so it opens **"Lamp Setup"** by itself
    within about 20 s. The ring stays dark while it waits.
  - **Old network still works, but she wants a different one:** unplug the
    lamp, plug it back in, and straight away hold the touch spot for 10
    seconds. It blinks three times and opens "Lamp Setup". It forgets
    nothing: the old network stays saved until she picks a new one there, and
    if she doesn't within 5 minutes, the lamp goes back to it. This only
    works in the first 30 seconds after plugging in, with a plain hold, so no
    other gesture can ever do it.

Done. 🎉

## Phase I — Secret gestures (optional)

For fun, with both lamps side by side as in Phase G. The guide's Lamp manual
tab plays each one.

- [ ] **39. Secret shows.** Tap A three times quickly, like knocking. When both
  lamps are dark again, tap A four times, then five times. Three taps: a white
  light blooms into a pastel rainbow on both lamps. Four: slow aurora
  curtains. Five: a white point bursts into rainbow and falls back in. A's tab
  prints `[tap] 3 taps -> secret`, then `[secret] playing rainbow`.
- [ ] **40. Secret holds.** On A: tap twice quickly, then press and hold for a
  few seconds, and let go. Then the same with three quick taps before the
  hold, then four. Two taps: the whole rainbow trails the light round both
  lamps. Three: two pink heartbeats, then the usual blue light. Four: stars
  come out and a wave joins them.
- [ ] **41. Two-person secrets.** **Mirror:** tap A three times quickly, then
  within 10 s tap B three times quickly. Both pulse together, then A turns
  from pink to mint while B turns from mint to pink. **Rally:** six single
  taps taking turns, each a few seconds after the last: A, B, A, B, A, B. On
  B's tap, a light runs round B, jumps to A, back to B and back to A, then
  both burst into rainbow.
- [ ] **42. Handshake and wish.** **Handshake:** tap A once; within 3 s, tap
  it twice quickly, then press and hold. A blue pulse, two white heartbeats,
  then a rainbow bloom, on both lamps. **Wish:** tap A five times quickly,
  then press and hold: a rare show on both, meteors, fireflies or a double
  rainbow, taking them in turn. If a rare show appears where you expected
  another secret, that's the every-fifth surprise; wait 90 s and try again.
- [ ] **43. Rave.** Tap A six or more times as fast as you can, and keep your
  finger down on the last one. Hold for about 15 s, then let go. Both lamps go
  neon: two beams whirl apart faster and faster, then a white blast, and every
  beat kicks the whole ring in a new colour, building and dropping again about
  every 6 s until you let go. A's tab prints `[tap] hold (rave) -> HOLD_START`.
  A plain blue hold instead means you pressed down more than 0.3 s after the
  last tap; A's tab printed `taps ignored`.

---

# Part 2 — Using the lamp

The lamp manual: everything the touch spot does, and exactly what each lamp
shows. The guide's **Lamp manual** tab has the same, with each gesture playing
on two simulated 24-LED rings that run the firmware's animation code. The lamp
shows three tones of one pale blue (glacier blue, cloud blue and cool white)
for everything everyday; soft pastels belong to the lamp light and the secret
gestures, and neon to rave alone. It's dark when nothing is happening. Both lamps work the same way;
"her lamp" means the other one.

### At a glance

| Gesture | Do | Means | Result |
|---|---|---|---|
| **Tap** | Touch the spot and let go | "Thinking of you" | Her lamp blooms, then breathes pale blue for 10 s |
| **Double tap** | Two quick taps while her tap is glowing on yours | "I got you" | Two white heartbeats run round both lamps |
| **Hold** | Keep your finger on the spot | "I'm here with you" | A light glides round her lamp for as long as you hold |
| **Hold together** | Both of you hold at the same time | "We're connected" | White waves roll round both lamps, in step |
| **Lamp light** | Double tap when nothing is glowing on yours | A light to live by | Your lamp becomes a moving aura until you double-tap again |

Three to five quick taps, sometimes ending in a hold, are
[secret gestures](#secret-gestures). Six or more on their own do nothing, so
fast tapping can never set off anything unexpected; only keeping your finger
down after them does: that's rave.

### Tap, double tap or hold?

- **Under 0.25 s is a tap.** The lamp then waits 0.3 s in case a second tap
  follows, so a tap is sent 0.3 s after you let go.
- **A second tap within 0.3 s of the first** makes a double tap.
- **0.25 s or longer is a hold**, lasting until you lift your finger.
- A quick tap right before a hold is dropped: only the hold is sent.
- **Three or more quick taps**, each within 0.3 s of the last and all within
  2.5 s, make a secret. The taps it's made of are never sent as taps.
- Taps keep turning into holds? Both timings are settings,
  `HOLD_THRESHOLD_MS` (250) and `DOUBLE_TAP_WINDOW_MS` (300) in
  `src/config.h`. Change them and reflash both lamps.

### Tap: "thinking of you"

- **Your lamp:** the ring brightens for a moment and a glacier-blue spark
  runs once round it, about 0.3 s after you let go. It means your tap was
  **recognised, not that it arrived**.
- **Her lamp:** the blue bloom. A cool-white highlight brightens at one spot
  on the ring, then splits and spreads both ways round it, leaving cloud blue
  behind; the two halves meet on the far side about 1.75 s in. The ring then
  breathes slowly between 35% and 60% brightness on a 4 s cycle, with a faint
  blue tint drifting round and now and then a glint of cool white, like light
  catching frost, until 10 s after the tap, then fades out over 3 s.
- **Tap again:** it blooms again over her glow and her 10 s restarts. If her
  lamp was already fading out, it blooms back up. Taps never stack.
- **Ignored when** her lamp is showing a hold, a hold-together, a secret or
  double-tap heartbeats. It isn't saved for later.
- **Did it arrive?** Only her double tap back tells you. Your own flash
  happens even with no internet.

### Double tap: "I got you"

- **Do it while her tap is glowing on your lamp**, including its 3 s
  fade-out.
- **Both lamps:** two cool-white heartbeats with pale-blue trails each run
  once round the ring, the second just behind the first, leaving a soft glow
  that fades out over 3 s. About 4.5 s in all, at the same moment on both,
  give or take the network.
- **Means** her tap arrived and you saw it: the only confirmation either of
  you gets.
- **With nothing to acknowledge** a double tap switches your
  [lamp light](#lamp-light) instead. Nothing is sent.
- **Afterwards** your lamp goes dark instead of finishing her tap's 10 s.
- **Ignored when** her lamp is showing a hold or hold-together, or is
  already showing heartbeats.

### Hold: "I'm here with you"

- **Do:** keep your finger on the spot. It becomes a hold at 0.25 s and lasts
  until you lift it, however long that is.
- **Your lamp:** a soft, dim glacier blue, so you can see the hold is live.
- **Her lamp:** the living current. A glacier-blue light glides round the
  ring once every 3 s, trailing cloud blue and the odd sparkle of stardust
  over a faint glow, for as long as you hold. When you let go it fades out
  over 3 s.
- **Takes over** a tap, double-tap heartbeats or a secret show on either
  lamp. Whatever it replaced doesn't come back: when the hold ends, the lamp
  fades to dark, or to its lamp light if that's on.
- **If you drop out:** your lamp tells hers "still holding" every second. If
  that stops because your power or WiFi went, her lamp ends the glow by itself
  within 3 s.

### Hold together: "we're connected"

- **Do:** both of you hold at the same time. Who starts first doesn't matter.
- **Both lamps:** a pale-blue glow, glacier shading into cloud, with two
  cool-white waves rolling slowly round it, once round every 9 s, sparkling
  where they crest. Both lamps show the same frame, kept in step using
  internet time. If either hold is a secret hold, both lamps show its colours
  instead (see [Secret gestures](#secret-gestures)).
- **Ends** when either of you lets go: both lamps fade out over 3 s.
- **The finger still down** does nothing more. Lift it and hold again to start
  a new hold.
- **Highest priority:** nothing interrupts it.

### Lamp light

- **Do:** double-tap when nothing is glowing on your lamp. Double-tap again to
  turn it off.
- **Your lamp** lights like a supernova: a white point gathers at one spot,
  bursts both ways round the ring and opens into an aura of soft mint, cyan,
  sky, lavender and pink, drifting slowly round. Always between 40% and 70%
  brightness, so it's a light to live by.
- **Off:** the aura falls back into the point and goes out.
- **Just yours.** Her lamp doesn't change, and nothing is sent.
- **Messages still come through.** Her tap, a hold or a secret plays over the
  aura, then fades back to it, exactly where it would have been.
- **Turns itself off** after 4 hours (`LAMP_AUTO_OFF_MS`), in case it's
  forgotten. A power cut turns it off too: lamps always start dark.
- **Her tap comes first.** While her tap is glowing on yours, a double tap
  acknowledges it, as always; only with nothing to acknowledge does it switch
  the light.

### Secret gestures

A hidden layer for when you're both playing, in pastel rainbow colours, so
the everyday gestures stay easy to read. Each secret is a quick run of taps,
each within 0.3 s of the last like knocking, sometimes ending in a hold. The
guide's **Lamp manual** tab plays every one of them.

| Do | Secret | Lights up |
|---|---|---|
| 3 quick taps | Rainbow bloom | Both lamps, 10 s |
| 4 quick taps | Aurora | Both lamps, 13 s |
| 5 quick taps | Supernova | Both lamps, 5 s |
| Tap; then 2 quick taps and hold | Secret handshake | Both lamps, 7 s |
| 2 quick taps, then hold | Rainbow current | Both lamps, while you hold |
| 3 quick taps, then hold | Heartbeat | Both lamps, while you hold |
| 4 quick taps, then hold | Constellation | Both lamps, while you hold |
| A secret hold, and she holds too | Together, in colour | Both lamps, while you both hold |
| You both do 3 quick taps, within 10 s | Mirror | Both lamps swap colours |
| Take turns: 6 single taps in a row | Rally | A light bounces between you |
| 5 quick taps, then hold | Wish | Both lamps: a rare show |
| 6 or more quick taps, then hold | Rave | Both lamps, while you hold |

- **Rainbow bloom:** like her tap bloom, but the light opens into a soft
  pastel rainbow that turns slowly round both lamps.
- **Aurora:** slow curtains of mint, cyan, sky, lavender and pink, drifting at
  different speeds so they never quite repeat.
- **Supernova:** a white point gathers, bursts round both rings into rainbow,
  shimmers, then collapses back into the point and goes out.
- **Secret handshake:** the three gestures it's made of, played back in order
  on both lamps: a glacier-blue pulse, two white heartbeats, then a rainbow
  bloom. The first tap still reaches her as an ordinary tap.
- **Rainbow current, Heartbeat, Constellation** are ordinary holds in another
  look, and play in full on both lamps, so the one holding sees them too. The
  rainbow current drags the whole rainbow round behind its light; the
  heartbeat opens with two pink-white heartbeats and beats again every 6 s;
  the constellation brings out stars one by one, then a blue-white wave joins
  them up. They still mean "I'm here with you".
- **Together, in colour:** hold-together takes the fancier of your two holds:
  rainbow waves, a shared pink heartbeat, a sky of stars, or the rave, in step.
- **Mirror:** you pulse together, then swap colours: lamp A starts pink and
  ends mint, lamp B the other way round.
- **Rally:** the first five taps are ordinary taps. On the sixth, a light runs
  round one lamp, across to the other, and back, a new colour each crossing,
  then both burst into rainbow.
- **Wish:** one of three rare shows, taken in turn: a meteor shower,
  fireflies, or a double rainbow. Every fifth tap-only secret in one go turns
  into one of these too.
- **Short memory.** Counts for the rally, the handshake, Mirror and the rare
  shows reset after 90 s of quiet or a power cut. Nothing is stored and no
  score is kept.
- **Rave:** hardcore at 150 bpm, the one time the lamps go neon. Two beams
  whirl apart from one spot, faster and faster, then the drop: a white blast,
  and every beat kicks the whole ring in a new colour while beams spin both
  ways and glitter pops. It builds and drops again about every 6 s for as long
  as you hold. It's an ordinary hold in another look, so hold-together raves
  too, in step. The whole ring flashes at most 2.5 times a second, under the
  3-a-second limit for people sensitive to flashing light. Mashing that went
  on longer than 2.5 s still counts.
- **Six or more taps do nothing** unless you hold after them, and an unfinished or mistimed secret is
  simply dropped. No secret can touch the WiFi or the pairing.
- **Offline:** a secret is only shown if it was sent, so your lamp never shows
  a shared moment on its own; you get the tap spark instead.

### When gestures overlap

Each lamp shows one thing at a time. The higher one wins:

1. Hold together
2. A hold, yours or hers
3. A secret show, from either of you
4. Double-tap heartbeats
5. Her tap, glowing
6. Dark, your lamp light, or fading out

A lower one arriving during a higher one is dropped, not saved for later. A
higher one replaces a lower one, which doesn't come back afterwards.

| If | Then |
|---|---|
| She taps while her last tap is still glowing on yours | It blooms again and her 10 s restarts; nothing stacks |
| She taps while yours is fading out | It blooms back up |
| You tap while her tap is glowing on yours | She gets your tap. Your flash shows over her glow, which carries on. |
| You both tap at the same moment | Each lamp shows the other's tap |
| She taps while you're holding | Not shown on yours, then or later |
| You start holding while her tap is glowing | Your hold takes over; when you let go, your lamp fades to dark |
| She starts holding while you're holding | Hold together |
| Her double tap arrives while you're holding | Ignored |
| You double-tap during her tap's fade-out | Still counts |
| Either lets go during hold-together | Both fade; the other finger's hold is used up |
| A message somehow arrives twice | The copy is dropped |

The reasoning is in [Gestures that overlap](#gestures-that-overlap).

### Offline and power cuts

- **Gestures are moments, not messages that wait.** A lamp that's unplugged
  or offline misses them, and starts dark when it comes back.
- **Your lamp offline:** a tap still flashes on yours, but isn't sent, now or
  later. That's why the flash doesn't mean "delivered".
- **Her lamp drops out during her hold:** yours ends the glow within 3 s, or
  at once if the server reports her lamp offline.
- **Your lamp drops out during her hold:** her glow ends on yours straight
  away.
- **After a power cut** each lamp rejoins WiFi by itself and starts dark.
- **Brightness tops out at 70%** of the lamp's maximum, for every animation.

### What the lamp is telling you

| Lamp shows | Meaning |
|---|---|
| Dark | Nothing happening. Also dark while starting up, joining WiFi or waiting in "Lamp Setup". |
| A blue spark, once round | Your tap was recognised |
| A white bloom, then pale blue breathing | She tapped: thinking of you |
| A blue light gliding round | She's holding her lamp right now |
| Soft, dim blue | You're holding yours |
| Two white heartbeats running round | A double tap: a tap got through |
| Pale blue with white waves rolling round | You're both holding |
| A slowly moving aura of soft colour | Your lamp light is on |
| Pastel rainbow colours | A secret gesture, from either of you |
| Three blue blinks | The WiFi-change hold worked; "Lamp Setup" opens next |

### The WiFi-change hold

The one hold that does something else. **Within 30 s of plugging the lamp
in**, make a plain hold of **10 s**: no taps before it, and not while she's
holding too. It starts as an ordinary hold, then blinks blue three times and
restarts into "Lamp Setup". **It forgets nothing:** the saved network stays
until a new one is chosen there, and if nobody chooses within 5 minutes, the
lamp goes back to it. After those first 30 s a hold is only ever a hold.
Step 38 has the details. Nothing a finger does can erase the WiFi; only the
`RESET_WIFI` serial command does.

### Try them from the computer

With a board plugged in and its monitor open (step 11), paste one of these
and press Return. Each acts as if a finger did it.

| Paste | Same as |
|---|---|
| `PING` | A tap |
| `ACK` | A double tap; only while the other lamp's tap is showing on this one |
| `HOLD`, later `RELEASE` | Holding, then letting go. `HOLD` on both boards gives hold-together. |
| `LAMP` | A double tap with nothing showing: the lamp light on or off |
| `TAPS 3` (or 4, 5) | That many quick taps: a secret show |
| `HOLD 2` (or 3, 4, 5, 6) | That many quick taps, then a hold: a secret hold, a wish with 5, or rave with 6 |
| `SHOW mirror` | Plays any show on both lamps: `rainbow`, `aurora`, `supernova`, `mirror`, `handshake`, `rally`, `meteors`, `fireflies`, `double-rainbow` |
| `STATUS` | Shows what the lamp is doing: `mode=idle`, `message`, `ack`, `holding`, `her-hold`, `together`, `fade`, `lamp`, or a show's name |

### Changing things

- **On the lamp, no cable needed:** the WiFi network (step 38).
- **Reflashing keeps the WiFi.** The board saves your network in a part of its
  memory an upload doesn't touch, and restarts by itself when the upload
  finishes. "Lamp Setup" only comes back if the board can't reach the saved
  network (it retries every 5 minutes), after `RESET_WIFI`, or after the
  10-second WiFi-change hold.
- **Needs a reflash:** gesture timing (`HOLD_THRESHOLD_MS`,
  `DOUBLE_TAP_WINDOW_MS`), the secret gestures' settings, the lamp light's
  auto-off, the brightness cap, the LED count and the broker. All of these
  are in `src/config.h`.

---

# Part 3 — Reference

## Parts list

This is the **ring version** of the Linked Lamp hardware, running our own
firmware. From Linked Lamp you only need the free 3D files. Quantities are for
**both lamps**. For each part, copy the search into Amazon, then check the
listing says what's in the last column before you buy.

### Buy in stages, so you never spend ahead of what's proven

| Stage | Buy | Then do | You've proven |
|---|---|---|---|
| 1 | 2 ESP32 boards, 1 USB cable (+ adapter on a USB-C-only Mac) | Phases C–D | The whole product works in software: two boards talking over the internet. No soldering. |
| 2 | Soldering iron kit, 1 pack of jumper wires, rings, touch sensors | Practise on jumper offcuts, then Phase E on board A | You can make the 3 joints, and a tap on A reaches B |
| 3 | Chargers, second cable, PLA (if needed) | Phases F–H | Finished lamps |

### Buy these

| Part | Qty | Type this into Amazon | Check the listing says |
|---|---|---|---|
| ESP32 board | 2 | `ESP32 ESP-WROOM-32 development board 30 pin USB-C` | **30 pins** (not 38), **USB-C** port, ESP-WROOM-32 (often called "DevKit V1"). **Not** ESP32-S3, -C3, -C6 or -S2: those are different chips. Pins already soldered on (sticking out underneath), so jumper wires can push onto them. A 3-pack is fine if it costs little more than 2. |
| LED ring | 2 | `WS2812B LED ring 66mm` | **66 mm outer diameter** (not 44 mm), WS2812B, 5 V. **16 or 24 LEDs both work**; if it's 16, step 13 has two settings to change. Already have **85–87 mm** 24-LED rings? Use the [bigger lamp files](#bigger-lamp-for-85-87-mm-rings) instead. |
| Touch sensor | 2 | `TTP223 capacitive touch sensor module` | TTP223. Usually a pack of 5–10, which is fine. |
| Jumper wires | 1 pack | `female to female dupont jumper wires 20cm` | **Female-female** (sockets on both ends), about 20 cm. You need 5 per lamp, plus a few to practise on; packs of 40 are typical. |

### Buy only if you don't already have one

| Part | Qty | Type this into Amazon | Check the listing says |
|---|---|---|---|
| USB charger | 2 | `USB wall charger 5V 2A` | **USB-A** socket (the rectangular one), **5 V 2 A** or more. An old phone charger with that on its label works. |
| USB cable | 2 | `USB A to USB C cable data sync 6ft` | USB-A to USB-C, and mentions **data** or **sync**, not just charging. Long enough to reach the lamp's spot. |
| USB adapter | 1 | `USB C to USB A adapter` | USB-C plug, **USB-A socket**. **Only if your computer has only USB-C ports** (most recent Macs); lets you flash with the cable above. |
| PLA, white | 1 spool | `PLA filament white 1.75mm` | Your printer's filament size (1.75 mm on most). For the diffusers. |
| PLA, dark | 1 spool | `PLA filament black 1.75mm` | Same size. For the bases. You need about 125 g of each colour in total. |

### Don't buy up front

| Part | Type this into Amazon | Only when |
|---|---|---|
| Resistor | `330 ohm resistor 1/4W` | The ring flickers or shows wrong colours. Try this first (see [Troubleshooting](#troubleshooting)). Linked Lamp's ring version uses none. |
| Capacitor | `100uF 16V electrolytic capacitor` | Flicker persists after the resistor. 100 µF, not 1000 µF (see [Troubleshooting](#troubleshooting)). |
| Diode | `1N4001 diode` | Flicker persists after both. Many rings never need any of these. |

### Tools, one-time (skip any you own)

A **soldering iron kit** often includes solder, a stand, spare tips and
sometimes wire strippers. Check what's in the kit before buying those
separately.

| Tool | Type this into Amazon | Check the listing says |
|---|---|---|
| Soldering iron | `soldering iron kit adjustable temperature` | **Adjustable temperature**. Avoid fixed-temperature irons. |
| Solder | `rosin core solder 0.8mm electronics` | **Skip if your iron kit includes solder.** Any **rosin-core (flux-core) electronics** solder, 0.6–1.0 mm: leaded (63/37 or 60/40, easiest to learn on) or lead-free (fine, just set the iron hotter). Never "acid core". Work with a window open and wash your hands afterwards. |
| Wire strippers | `wire stripper 20-30 AWG` | Goes down to **26–28 AWG**: jumper wires are thin |
| Multimeter | `digital multimeter` | Any basic one; for checking connections when something doesn't light |
| Helping hands | `helping hands soldering station` | Clips to hold parts while you solder. Strongly recommended. |

Practise soldering on the leftover halves of jumper wires; you don't need to
buy perfboard or hookup wire.

### Don't buy (these are for Linked Lamp's other version)

If you're looking at Linked Lamp's own parts list, skip the **custom PCB,
RGB LEDs, 2N2222 transistors, 15 Ω / 22 Ω resistors, 90° header pins and NFC
tag**. They're for its PCB version, which our firmware doesn't drive.

### Optional: OLED screen (not needed for the lamps)

A small screen in the base showing her local time, her last tap, days together
and a tap count. The lamps work fine without it, and **the firmware for it
isn't written yet**; ask when you're ready. See the
[wiring](#optional-oled-screen) and the [printed holder](#3d-print-notes).

| Part | Qty | Type this into Amazon | Check the listing says |
|---|---|---|---|
| OLED screen | 2 | `0.96 inch OLED I2C SSD1306 128x64 4 pin` | **4 pins labelled VCC, GND, SCL, SDA** (not the 7-pin type). Works at 3.3 V. Often sold in packs of 2–3. |
| Heat-shrink or tape | 1 pack / roll | `heat shrink tubing assortment` or `electrical tape` | Either works, to cover the two wire joins per lamp |

### Why these choices

- **ESP32 board:** any ESP32-WROOM-32 dev board runs the firmware, but the
  enclosure is laid out for the 30-pin "V1" board with USB-C: it sits upside down on four locating pins 47 × 22.9 mm
  apart, with its USB-C socket against a slot in the base. Another
  board may not line up.
- **Power, the same as Linked Lamp:** a USB cable into the ESP32 powers
  everything. The ring draws 5 V from the board's `VIN` pin, which is the USB
  5 V after a protection diode (about 4.7 V), not the 3.3 V regulator.
- **Why USB-A to USB-C cables:** a USB-C charger or computer port only turns
  on power if the device has two 5.1 kΩ resistors on its CC pins, and some
  cheap ESP32 boards leave them off. On a C-to-C cable those boards stay dead.
  A USB-A port always supplies 5 V, which is why the charger is USB-A and why
  a USB-C-only computer needs the adapter.
- **Why 66 mm, and 16 or 24 LEDs:** 66 mm is what fits the base. Linked Lamp
  specifies 16 LEDs because the ring is powered through the board, but 66 mm
  rings on Amazon are mostly 24 LEDs. Either works. `config.h` is set up for 24 (`RING_PIXELS 24`,
  `MAX_BRIGHTNESS 120`); for 16, use 16 and **180**. Both draw the same
  current, so the lamp looks about as bright either way.
- **Why no resistor, capacitor or breadboard:** Linked Lamp's ring version
  uses none. They're common insurance against flicker on long wires or big
  power supplies; with 10 cm wires on USB power they rarely matter. Leaving
  them out means every connection is a single jumper wire, and the only
  essential soldering is 3 joints on the ring. If your ring does flicker, add
  them in the order the "don't buy up front" table gives.

## Wiring

Six wires, each from one part straight to one ESP32 pin. The **[visual wiring
guide](https://claude.ai/artifact/NWUZS2aGTcL2LgVJx6mHmB#wiring)** draws this
with the board's real pin layout, plus step-by-step soldering pictures.

Find each ESP32 pin by **counting from the USB port** down the side it's on.
That works whichever way you hold the board. It's the same wiring as Linked
Lamp's schematic, which draws the board with the USB port at the top.

| Wire | ESP32 pin | Where it is | Goes to |
|---|---|---|---|
| 1 | `VIN` | 1st pin from the USB port | Ring `5V` (or `VIN`) |
| 2 | `GND` | 2nd pin, same side as VIN | Ring `GND` |
| 3 | `D27` | 6th pin, same side as VIN | Ring `DIN` (or `DATA`) |
| 4 | `3V3` | 1st pin from the USB port, other side | Sensor `VCC` (or `VIN`) |
| 5 | `GND` | 2nd pin, same side as 3V3 | Sensor `GND` |
| 6 | `D4` | 5th pin, same side as 3V3 | Sensor `I/O` (or `IO`) |

The USB cable plugs into the ESP32 and powers everything.

Two mix-ups to avoid: the ring's power goes to `VIN`, **never** `3V3`; and the
sensor's power goes to `3V3`, not `VIN`. They're both first from the USB port,
on opposite sides. `D27` is the firmware's GPIO 27 and `D4` is GPIO 4. The
board has two `GND` pins, connected inside, so each part gets its own and no
wires need joining.

Ring pad names vary: 5V may be `VCC`, `VIN` or `+5V`, DIN may be `DATA`, `DI` or
`IN`, DOUT may be `DO` or `OUT`. The sensor's VCC may be printed `VIN`, and its
I/O may be `IO`, `SIG` or `OUT`. Pin order varies too, so always go by the
printed labels. Linked Lamp's own schematic draws the board rotated 180° (USB
port at the top); it's the same six wires.

**Power, in one line:** a 5 V 2 A charger per lamp is plenty, as long as the
ring is on `VIN` and a 24-LED ring has `MAX_BRIGHTNESS 120`. The numbers
behind that are in [Power](#power), under How it works; you don't need them to
build.

**When reflashing from a computer,** the computer's USB port may supply only
0.5 A. If the board resets while the ring is lit during flashing, that's why,
and it's harmless: flash with the lamp dark.

**Joint count:** **3 per lamp** on the ring's pads, plus 3 on the sensor if
it came without pins. The ESP32 needs no soldering, because female
jumper wires push onto its pins. The GPIO numbers match Linked Lamp's ring
version.

### Optional: OLED screen

Only if you add the [optional screen](#optional-oled-screen-not-needed-for-the-lamps).
It needs 4 wires. Its two data wires go to free pins on the `3V3` side of the
board. Its power has to share the sensor's `3V3` and `GND` pins, because a pin
only takes one jumper socket, so those two wires get joined.

| Wire | ESP32 pin | Where it is | Goes to |
|---|---|---|---|
| 7 | `D21` | 11th pin from the USB port, same side as 3V3 | Screen `SDA` |
| 8 | `D22` | 14th pin from the USB port, same side as 3V3 | Screen `SCL` |
| — | joined onto wire 4 (`3V3`) | | Screen `VCC` |
| — | joined onto wire 5 (`GND`) | | Screen `GND` |

```
ESP32 3V3 ──┬── sensor VCC        ESP32 D21 ── screen SDA
            └── screen VCC        ESP32 D22 ── screen SCL
ESP32 GND ──┬── sensor GND        (the GND next to 3V3)
            └── screen GND
```

**Making a Y-join:** cut the sensor's wire (4 or 5) in the middle. Strip 3 mm
off both cut ends and off one half-jumper whose socket will go on the screen.
Twist the three bare ends together, solder them, and cover the join with
heat-shrink or tape. Do the same for the other wire. Each join is one solder
joint.

The screen draws about 10–20 mA, which makes no difference to the power
budget.

## Config settings

All settings are in `src/config.h`. The comments in `src/config.example.h`
explain how to choose each value.

| Setting | Default | Notes |
|---|---|---|
| `DEVICE_ID` | `"A"` | `"A"` on one lamp, `"B"` on the other. **This is the only difference between the two builds.** |
| `RING_PIXELS` | 24 | Count your LEDs |
| `MAX_BRIGHTNESS` | 120 | 0–255, the USB power cap: keep `RING_PIXELS` × this at or below 2880 (24 LEDs: 120; 16 LEDs: 180). Animations peak at 70% of it. |
| `HOLD_THRESHOLD_MS` | 250 | How long a touch must last to count as a hold. Raise it towards 400 if taps turn into holds. |
| `DOUBLE_TAP_WINDOW_MS` | 300 | How long the lamp waits for another tap. It's also the delay before a single tap is sent. Raise it towards 450 if secret taps keep splitting into separate taps. |
| `LAMP_AUTO_OFF_MS` | 4 hours | The lamp light turns itself off after this long. 0 keeps it on until the next double tap. |
| `SECRETS_ENABLED` | 1 | 0 keeps only the everyday gestures. Set it the same on both lamps. |
| `SEQUENCE_WINDOW_MS` | 2500 | The longest a run of secret taps may take, first press to last release. |
| `HANDSHAKE_STEP_MS` | 3000 | After a tap, how long the handshake waits for "2 quick taps, then hold". |
| `MIRROR_WINDOW_MS` | 10000 | How close together your two 3-tap secrets must be to make Mirror. |
| `RALLY_TAPS`, `RALLY_GAP_MS` | 6, 20000 | Single taps taking turns that set off the rally, and the longest wait for each reply. |
| `SESSION_TIMEOUT_MS` | 90000 | Quiet this long and the secret counters reset. |
| `SESSION_RARE_EVERY` | 5 | Every this-many-th tap-only secret in a session is a rare show instead. 0 turns it off. |

An older `config.h` may still have settings that no longer exist
(`HOLD_SECONDS`, `TOGETHER_…`, `RAINBOW_…`, `QUIET_…`, `TZ_STRING`,
`DEFAULT_R/G/B`). They're ignored, and the two gesture settings above fall back
to their defaults if they're missing.

## Serial commands

Open the monitor from the project folder, so it picks up the 115200 baud
setting from `platformio.ini`:

```bash
cd ~/Projects/lamp_proj
pio device monitor
```

Type a command and press Return. What you type is echoed back
(`monitor_echo` in `platformio.ini`). Press Control C to close the monitor.

| Command | Does |
|---|---|
| `STATUS` | Prints the current mode, WiFi, MQTT, signal strength, clock sync, and who's holding |
| `PING` | Same as a single tap |
| `ACK` | Same as a double tap (only does something while a tap from the other lamp is showing) |
| `HOLD` | Same as starting a hold; stays held until `RELEASE`. `HOLD 2` to `HOLD 6` add that many quick taps first; `HOLD 6` raves. |
| `TAPS 3` | Same as that many quick taps (1 to 9) |
| `LAMP` | Turns the lamp light on or off, like a double tap with nothing showing |
| `SHOW aurora` | Plays any secret show on both lamps, as if recognised here |
| `RELEASE` | Ends a `HOLD` |
| `RESET_WIFI` | Wipes WiFi credentials and reopens "Lamp Setup" |

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Board doesn't appear as a serial port | Charge-only USB cable, or missing driver | Swap the cable. Read the small chip next to the board's USB port: **CP2102** needs the Silicon Labs CP210x driver; **CH340 / CH9102** needs the WCH driver. Reboot after installing. |
| Upload stuck at `Connecting.....` | Board didn't enter flashing mode | Hold the **BOOT** button while the upload starts, release when it shows progress |
| `Unable to verify flash chip connection` right after `Changing baud rate` | Upload speed too fast for the board's USB chip or cable | In `platformio.ini`, change `upload_speed` to `115200` and upload again |
| Board gets no power on a USB-C-to-C cable | Board lacks the USB-C CC resistors | Use a USB-A-to-C cable (with an adapter on a USB-C-only computer) |
| Computer says a USB device needs too much power | Too much capacitance or current on USB | If you added a capacitor, use 100 µF, not 1000 µF; flash with the lamp dark |
| `command not found: pio` | Terminal window opened before PlatformIO was installed | Quit Terminal (⌘ Q) and reopen it. Still missing: run `uv tool update-shell`, then reopen. |
| `externally-managed-environment` | Installed with `pip` | Use the uv commands in step 5 instead |
| Monitor shows garbled characters | Monitor opened outside the project folder, so at the wrong speed | `cd ~/Projects/lamp_proj` first, or add `--baud 115200` |
| `rc=-2` in the monitor | Certificate wrong, missing or still the placeholder, or `MQTT_HOST` wrong | Re-download the CA certificate from the EMQX deployment's Overview page and paste it again; check the address |
| `rc=-2` on a lamp that used to work | The EMQX deployment has stopped: the free monthly allowance ran out, or no lamp connected for 30 days | Check the deployment in the EMQX console |
| `rc=4` or `rc=5` | Wrong broker credentials | Check `MQTT_USER` / `MQTT_PASS` against Access Control → Client Authentication |
| Won't join WiFi | Router is 5 GHz only | Use a 2.4 GHz network |
| Ring never lights for anything | Can't join the saved WiFi (waiting in "Lamp Setup"), not connected to the broker, or a ring wiring fault | Check the monitor for `[mqtt] ... connected`; if it's waiting in "Lamp Setup", join it from a phone and choose the network |
| Diffuser prints only its flat top plate | Its print profile uses Bambu's default wall generator, Classic, which drops the 0.4 mm walls | Open the Diffuser profile in Bambu Studio, turn on **Advanced** on the Process row, set **Quality → Wall generator** to **Arachne**, slice (the slider on the right should top out at about 509), save, and replace the Diffuser profile on MakerWorld (step 18, action 7) |
| Taps arrive as holds, or double taps as two taps | Gesture timing doesn't suit your touch | Raise `HOLD_THRESHOLD_MS` or `DOUBLE_TAP_WINDOW_MS` in `config.h` a little and reflash |
| Flicker, wrong colours, first pixel misbehaving | Signal ringing on the data wire, or 3.3 V data into a ring powered at about 4.7 V: borderline. Many rings never do this. | Fix in this order, testing after each: **(1)** a **330 Ω resistor** inline in the DIN wire, near the ring; **(2)** a **100 µF capacitor** across the ring's 5V and GND pads, with its striped leg to GND; **(3)** a **1N4001 diode** in the 5V wire, stripe towards the ring, so the ring runs at about 4 V. |
| Resets when the ring lights up | Weak charger or cable, or ring wired to `3V3` | Use a 2 A charger and a short, decent cable; ring on `VIN`, never `3V3`. Last resort: lower `MAX_BRIGHTNESS`. |
| Touch doesn't respond, or fires on its own | Sensor calibrated while touched, or A/B pads bridged | Unplug, replug without touching it; check the A/B pads are open |
| Intermittent faults | Cold solder joints | Reflow them |
| Some pixels dark | `RING_PIXELS` too low | Count again |
| Lamp stuck | Anything | Type `STATUS` in the monitor; the watchdog reboots it after 30 s of hanging |

## Soldering tips

1. **Heat the joint, not the solder.** Touch the iron to the pad and wire
   together for 2 seconds, *then* feed solder into the joint. Melting solder
   onto the tip and dabbing it on makes a cold joint that looks fine and fails
   a month later.
2. **Good joints are smooth and concave**, like a small volcano hugging the
   wire. If it's blobby, ball-shaped or doesn't wet the pad, redo it. Leaded
   joints are shiny; lead-free joints are naturally duller and slightly grainy,
   which is fine.
3. **Set the iron's temperature for your solder:** about **330 °C** for leaded
   (63/37, 60/40) and about **360 °C** for lead-free. It's printed on the
   solder's label or spool.
4. **Keep the tip tinned.** A dry, oxidised tip transfers no heat, and you'll
   think the iron is broken.

Watch EEVblog's soldering tutorial. CoronaLamps Steps 2–3 have good close-up
photos of finished joints.

## 3D print notes

The files are from Aarush Magic (the Linked Lamp author). They're licensed
**CC BY-SA 4.0**, which doesn't matter for personal use, but share-alike
applies if you publish a remix. They ship as **STEP** files, which open as
editable solids in Fusion 360, Onshape or FreeCAD.

| Part | Footprint | Height |
|---|---|---|
| `Neopixel_Lamp_-_3in_Diffuser.step` | Hexagon, 91 mm across flats / 105 mm across corners | 76.2 mm |
| `Neopixel_Lamp_-_Base__Spokes_.step` | Same | 34 mm |

The assembled lamp is about 105–110 mm tall. The diffuser is intentionally
faceted, with 464 flat faces.

**Mounting features in the base** (Linked Lamp's, and the same in the
bigger lamp):

| Feature | Size | What it's for |
|---|---|---|
| Ring seat | ⌀69 mm, 1.6 mm deep (88 mm in the bigger lamp) | The LED ring, with a wire notch at each end |
| 4 posts with pins | ⌀3.18 mm posts, ⌀2.03 mm pins, on a 47 × 22.9 mm rectangle | The ESP32 sits upside down on them, pins through its mounting holes. No screws. |
| USB-C slot | 8.7 × 3.6 mm, near the bottom of one flat face | The USB-C cable plugs into the ESP32 through it |
| Touch circle | ⌀7.4 mm, 0.2 mm deep, on the opposite flat face | Marks where to touch. It isn't a hole. |
| Touch sensor slot | 11.3 mm wide, open at the top, just inside that face | The sensor slides down into it, touch side facing the wall |

- **Ring size:** the 69 mm bore fits a **66 mm** ring. Linked Lamp specifies
  up to 16 LEDs at that size. Check the diameter when buying: the common
  16-LED Adafruit ring is only 44.5 mm and would rattle. 24-LED rings are also
  sold at 66 mm; they fit, but need `MAX_BRIGHTNESS 120` (see
  [why](#why-these-choices)). The ring is an officially supported Linked Lamp
  variant, wired to the same pins our firmware uses (data on GPIO 27, touch on
  GPIO 4).
- **Touch sensor:** it slides into the slot behind the touch circle (see the
  table above), so it needs no glue.
- **Print settings:** 0.2 mm layers, 2 walls, 15% infill (about 3.5 h). Print
  the diffuser in white and the base in a dark colour. Don't use the
  CoronaLamps 10%-infill hex trick here, because it clashes with the faceting.
- **Optional screen: printed holder, no cutting.** Set `MAKE_SCREEN_VERSION`
  to `True` in `3d-design/settings.py` and rebuild. A base with the screen
  window appears in `3d-prints/3 Optional - base with screen window`; print it
  instead of the plain one. It has a 24 × 13 mm bevelled window in the angled
  face next to the touch side, and a slot behind it that holds a 0.96" OLED
  module. The module goes in from the hollow inside: pins at the top, glass
  facing the window. Push it forward into the slot until it drops about 1 mm;
  that drop stops it sliding back out, so no glue is needed. **Print the screen
  fit test in the same folder first**, a half-hour slice of the same wall, and
  check the module slides in and drops into place. Not test-printed yet.
- **Expect to find problems yourself.** Only a handful of people have printed
  these files, so budget for a reprint.

### Bigger lamp for 85–87 mm rings

For 24-LED rings with an 85–87 mm outside diameter (about 70 mm inside),
which are too big for Linked Lamp's 69 mm ring seat. `3d-design/make_lamp.py`
makes a base and diffuser about 1.32× bigger, so the ring sits in the same
proportions and the glow looks the same: about 120 mm across and 143 mm tall
assembled. Everything that has to fit a real part keeps its original size and
position: the ESP32's posts and USB-C slot, the touch sensor's slot and touch
circle, and the fit between diffuser and base. The diffuser's walls are one
0.4 mm printed line, as in the original, so its print profile uses Bambu's
**Arachne** wall generator: the default (Classic) drops walls that thin and
prints only the top plate. Linked Lamp's own file does the same with default
settings. A thicker wall can't fix it without changing the base: Classic
prints two overlapping lines in anything under about 0.84 mm, and the gap
between the rim and the outside is 0.5 mm. The ring seat is 88 mm across,
so an 87 mm ring drops in with 0.5 mm to spare each side and an 85 mm one has
1.5 mm of play.

The top is engraved 0.4 mm deep with **"Pookie"** in Pacifico, a bold script
font, with a heart dotting the i and one either side. The engraving is thinner
than the rest of the top, so it glows brighter when the lamp is lit. It prints
face down and is mirrored to suit, so it reads correctly on the lamp; turn the
diffuser on the base so the word faces the front. To engrave something else,
change `TOP_TEXT` in `3d-design/settings.py` and rebuild; `TOP_TEXT = ""`
brings back Linked Lamp's logo. Lines thinner than about 0.8 mm don't print,
which is why the font is a bold one.

| Folder | What it's for |
|---|---|
| `3d-design` | **What you edit.** `settings.py` holds the ring size and the few other values you'd change; `HOW TO CHANGE THE LAMP.txt` walks through it. The scripts build everything in `3d-prints` from these settings. |
| `3d-prints/1 Print these` | What gets printed, in order: a fit test (1), the diffuser (2) and the base (3) |
| `3d-prints/2 MakerWorld upload kit` | Listing text to copy, pictures, and a folder for the print profiles you save from Bambu Studio |

Printing is covered in [Phase F](#phase-f--print-and-assemble). Expect about
twice the plastic and print time of the original. Firmware settings are the
same as any 24-LED ring: `RING_PIXELS 24` and `MAX_BRIGHTNESS 120`.

**If the ring changes again:** edit `3d-design/settings.py` and run this from
the project folder. It rebuilds every file in `3d-prints`, including the
pictures and the sizes quoted in the listing text:

```bash
cd ~/Projects/lamp_proj
uv run --no-project --python 3.12 --with cadquery python 3d-design/make_lamp.py
```

With `TOP_TEXT = ""`, the script needs Linked Lamp's diffuser in
`3d-design/linked-lamp-original`, because its logo is copied from there. That
file is kept out of git; if it's missing, download `Neopixel_Lamp_-_3in_Diffuser.step` from the `3D Models/`
folder of [Linked-Lamp](https://github.com/aarushmagic/Linked-Lamp) and save it
there as `Neopixel Lamp - 3in Diffuser.step`.

Not test-printed yet, so print the fit test first.

## Repo layout

```
├── platformio.ini
├── README.md
├── CLAUDE.md               ← rules for anyone (or any AI) editing the code
├── .gitignore
├── 3d-design/               ← what you edit to change the lamp's size
│   ├── HOW TO CHANGE THE LAMP.txt
│   ├── settings.py         ← ring size and the few other values you'd change
│   ├── make_lamp.py        ← builds everything in 3d-prints
│   ├── screen_holder.py    ← optional screen window
│   ├── top_text.py         ← the text and hearts engraved on the top
│   ├── fonts/              ← Pacifico, the script font (open font license)
│   ├── make_pictures.py    ← listing pictures
│   ├── paths.py            ← folder and file names in 3d-prints
│   └── linked-lamp-original/  ← Linked Lamp's diffuser (gitignored; its logo, if TOP_TEXT is empty)
├── 3d-prints/              ← generated: what you print and upload (gitignored)
│   ├── READ ME FIRST.txt
│   ├── 1 Print these/
│   └── 2 MakerWorld upload kit/
└── src/
    ├── main.cpp            ← all the firmware
    ├── config.example.h    ← template; edit this when adding a setting
    └── config.h            ← your copy with passwords, gitignored
```

---

# Part 4 — How it works

## Protocol

Both lamps connect to the same MQTT broker over TLS.

```
lamp/<PAIR_ID>/A/signal     A publishes, B subscribes
lamp/<PAIR_ID>/B/signal     B publishes, A subscribes
lamp/<PAIR_ID>/A/status     "online" / "offline" (last will), retained
lamp/<PAIR_ID>/B/status     each lamp watches the other's
```

```json
{"type":"TAP","id":"A-1a2b3c4d-7","rally":2}
{"type":"ACK","id":"B-9f8e7d6c-3","of":"A-1a2b3c4d-7"}
{"type":"HOLD_START","id":"A-1a2b3c4d-8","hold":"A-1a2b3c4d-8","look":"rainbow"}
{"type":"HOLD_END","id":"A-1a2b3c4d-12","hold":"A-1a2b3c4d-8"}
{"type":"SECRET_GESTURE","id":"A-1a2b3c4d-13","fx":"aurora","seed":2779096485,"lead":"A","at":1792566909123}
```

`PAIR_ID` namespaces your topics so nobody else on the broker collides with
you. Topics derive from `DEVICE_ID`, so both lamps run identical firmware.

- **Nothing on the signal topic is retained.** Gestures are moments, not
  state, so a lamp that was unplugged misses them and boots dark. On connect,
  each lamp clears anything the old firmware left retained there.
- **Every message has a unique `id`**: the lamp, a random number chosen at
  each boot, and a counter. A lamp drops any id it has already seen, so a
  message delivered twice can't restart an animation.
- **Only a double tap confirms delivery.** Your lamp's spark means your tap
  was recognised. The firmware logs whether the tap left your lamp, but the
  light never claims it arrived until her `ACK` comes back.
- **Ids are checked.** An id that doesn't start with the other lamp's
  `DEVICE_ID` isn't hers, and is ignored.
- **The lamp light sends nothing.** It's local to each lamp.

## Holds: live, with a heartbeat

A hold has to be known *while it's happening*, and its end can get lost: her
lamp can lose power or WiFi before `HOLD_END` goes out. So while a finger is
down, the lamp repeats `HOLD_START` every second, all with the same `hold` id.
The other lamp starts its glow only for a hold id it isn't already showing,
so the repeats just keep it alive. If they stop for 3 s, it ends the hold
itself and fades out. It also ends it at once if the broker reports her lamp
offline, or if it loses its own connection.

## Hold together: no coordinator

No server decides that you're both holding. Each lamp knows two things: "my
finger is down" and "her hold is live". Both lamps see the same two facts, so
both reach the same answer alone. The waves take their position from the
shared NTP clock, so both lamps draw the same frame at the same moment.

When either of you lets go, both lamps fade out. The hold that's still down
is then **spent**: that lamp sends `HOLD_END` and stops repeating, and the
other lamp ignores the rest of that hold. Otherwise the remaining finger
would carry on as a solo hold and light the other lamp straight back up.

## Gestures that overlap

Exactly one mode drives the LEDs at a time, chosen by priority:

1. Hold together
2. A live hold (hers, or yours, in any look)
3. A secret show
4. Acknowledgment (double tap)
5. Tap message
6. Idle or the lamp light, or a fade (anything may interrupt these)

The lamp light isn't "resumed" after an interruption: while it's on, it is
what the ring rests on instead of dark, so a fade ends in it.

A lower-priority event never interrupts a higher one and isn't saved for
later. A higher one replaces a lower one outright, and whatever it replaced is
dropped, never resumed. Every change starts from what's actually on the ring,
so transitions are smooth.

| Situation | Result |
|---|---|
| She taps while a tap from her is showing | It blooms again and the 10 s restarts; nothing stacks |
| You double-tap while her tap is showing | Both lamps run two white heartbeats, then fade |
| A tap or double tap arrives during any hold | Ignored |
| A hold starts while a tap is showing | The hold takes over; the tap doesn't come back afterwards |
| A hold starts during a fade | Blends from the fade straight into the hold |
| Either of you lets go during hold-together | Both lamps fade out; nothing resumes |
| A message arrives twice | The copy is dropped |

## Secrets

**Decided once, by the lamp that saw them.** The lamp whose sensor recognised
a secret works out which show it is and publishes `SECRET_GESTURE`; the other
lamp just plays it, so the two never disagree. `at` is a moment 0.25 s ahead
on the shared internet clock, so both lamps start together; one arriving more
than 4 s late is dropped. `seed` makes both lamps' random patterns (stars,
meteors, fireflies) identical, and `lead` says whose lamp goes first in a
rally. A show is only played if it went out, so an offline lamp never shows a
shared moment alone.

**Secret holds are ordinary holds with a `look`.** Heartbeats, timeouts and
hold-together work exactly as for a plain hold; hold-together shows the higher
of the two looks, and both lamps see both looks, so both pick the same one.
The holder's lamp plays the look in full too, so the fun is on both ends.

**Mirror is worked out on each lamp**, like hold-together: each sees its own
3-tap and hers within 10 s. A rally travels inside ordinary taps: each `TAP`
carries how many taps in a row have taken turns, and the lamp that sends the
sixth publishes the rally instead.

**The handshake** starts with an ordinary tap, which goes out as usual. "2
quick taps, then hold" within 3 s of it completes the handshake instead of
starting a rainbow hold. The finger that completes it, or makes a wish, is
spent until it lifts, like the hold left after hold-together.

**Sessions are short and forgetful.** Rally turns, the handshake's step,
Mirror's window and the rare-show count live in memory only, and reset after
90 s without an interaction or on a power cut. No dates, streaks or
statistics, and duplicates never count twice.

**Why taps, not new gestures.** The sensor only knows touch and release, so
the secrets are counts of quick taps, with or without a closing hold. 1 and 2
taps stay the everyday gestures, and a tap just before a hold is still
dropped so a fumbled hold stays a hold. "Tap, tap, then hold" was proposed for
both the rainbow current and the constellation, so the constellation moved to
4 taps; a handshake told apart from "3 taps, then hold" by rhythm would be too
fragile on this sensor, so it starts with a separate tap instead. Rave took
"6 or more, then hold" because every slot from 2 to 5 was taken, and because
a stray tap before a hold must stay harmless. Mashing alone is still
ignored; the closing hold is what makes it deliberate. It skips the 2.5 s
limit on purpose: a run of 6+ taps each within 0.3 s can only be mashing.

## The light

For everything everyday, three tones of one pale blue: glacier blue
`(168, 223, 255)` for the main glow, cloud blue `(214, 238, 255)` for softer
gradients and trails, and cool white `(235, 247, 255)` for highlights. Idle is
dark. The secrets add a pastel rainbow: pink, peach, butter, mint, soft cyan,
sky blue and lavender, all softer than cool white, so they never draw more
power. Rave alone goes neon: magenta, violet, electric blue, aqua, lime and
yellow (`NEON`), fully saturated but one or two channels each, so it still
draws less than white. No pure red, the worst colour for photosensitive
viewers. Its beat (`RAVE_BEAT_MS`, 400 ms) is the whole-ring flash rate and
must stay above 334 ms, 3 flashes a second. The lamp light is a slowly moving
aura of the cool pastels, between 40% and 70%, on its own clock from
switch-on.

Every animation draws each of the 24 LEDs separately, so light moves round
the ring instead of the whole ring changing at once. Positions are fractional,
and a moving light is a soft bell about four LEDs wide, so it glides between
LEDs instead of hopping from one to the next. Every change of mode blends
from what's on the ring; the bloom and the heartbeats sweep their new picture
in behind their moving light.

`writeRing()` is the only code that writes the LEDs. It caps each LED's
brightest channel at 70% of `MAX_BRIGHTNESS`, then gamma-corrects and scales
in one step, rounding once so the dim ends of fades and trails don't drop to
black early. Frames are drawn every 16 ms (about 60 per second) without
blocking, so touch and MQTT stay responsive.

## Power

Background only; the parts list and step 13 already apply it. The ring takes
5 V from the ESP32's `VIN` pin, which is the USB supply passed straight
through, so the charger powers everything.

| What | Draw |
|---|---|
| 24-LED ring at brightness 120 (or 16-LED at 180, the same draw), every LED glacier blue at the 70% limit | About 0.4 A |
| Same ring, every LED cool white at the 70% limit (no animation lights the whole ring this white) | About 0.46 A at most |
| Same ring, full white at full brightness (never shown) | About 0.7 A |
| ESP32, including WiFi peaks | About 0.1–0.25 A |

The worst case is under 1 A: within what a 2 A charger and the board's USB
power path can supply, and lower than Linked Lamp's own brightness. That's why
a 24-LED ring needs brightness lowered to 120, and why the ring must never draw
from the `3V3` pin, whose regulator can't supply it.

## Reliability

| Requirement | How |
|---|---|
| WiFi drops | Reconnects; full reboot after 5 min down |
| Broker drops | Retries with backoff (5 s → 60 s); resets WiFi after 6 failures |
| Firmware hang | 30 s hardware watchdog reboots it |
| Power loss | Boots dark; nothing from before is replayed |
| Offline taps | Your lamp still flashes; nothing claims the tap arrived |
| Lost hold release | The other lamp ends the hold after 3 s without a heartbeat |
| Duplicate messages | Dropped by id |
| She moves house | Replug: "Lamp Setup" opens by itself when the saved WiFi is gone. Otherwise replug and make a 10 s hold straight away. No cable needed, and nothing is forgotten until a new network is chosen. |
| Secret taps go wrong | An unfinished or mistimed secret is dropped; nothing a finger does touches WiFi or pairing |
| Lamp light forgotten | Turns itself off after 4 hours, and a power cut always starts dark |
| Never blocks | No `delay()` after startup; everything runs off a 60 fps timer loop |

---

# Part 5 — Design decisions

Background for anyone wondering "why not just…". These are settled; see
CLAUDE.md before reopening any of them.

## Who this was designed for

A software engineer (JS/TS, Node, AWS, comfortable with MQTT and C syntax) with
**no** soldering, electronics or CAD experience, who owns a 3D printer. So the
firmware is the easy part and the physical build is the risky part. The most
likely way this fails is being abandoned at the enclosure stage, which is why
the enclosure is downloaded rather than designed and the build order front-loads
the software milestone.

## Sources

| Source | Taking | Not taking |
|---|---|---|
| [Linked-Lamp](https://github.com/aarushmagic/Linked-Lamp) | **3D print files**, and its ring version's hardware: board, pins, USB power | Firmware, browser flasher, web app, OTA, PCB version |
| [circuitcindy](https://projecthub.arduino.cc/circuitcindy/wifi-enabled-friendship-lamps-e2577b) | The mutual-tap (cuckoo) **idea** | Her code and stack |
| [CoronaLamps](https://www.instructables.com/CoronaLamps-Simple-Friendship-Lamps-Anyone-Can-Mak/) | Soldering photos | Everything else, see below |
| [juekr](https://github.com/juekr/remote-friendship-lamp) | Nothing | 60 s polling, disabled TLS validation |
| [patrick.ble.si](https://patrick.ble.si/wifi-touch-light) | Nothing | Particle Photon is discontinued |

## Why our own firmware instead of Linked Lamp's

Linked Lamp's conveniences are all hosted on the author's domain. The browser
flasher has you type your MQTT password into his page, the web app loads from
his server and holds your broker credentials, and OTA updates give his domain
code execution on a device inside your home network.

None of that implies bad faith; the design deliberately keeps him away from
your broker. But the repo has no stars or forks, nobody has audited it, and
code served from someone else's domain can change at any time. Auditing
thousands of lines of his C++ costs more than writing about 500 of our own.

We use his 3D files because an STL or STEP file is just geometry. It can't
execute anything. **We take the part with zero trust surface and write the
part that needs trust ourselves.**

## Why EMQX Serverless

It's free for two lamps and needs only a username and password over TLS.
They use about 9% of the monthly connection allowance and a few percent of
the 1 GB traffic allowance. With the spend limit at $0 it can't bill you: if
the allowance ever runs out, the deployment stops instead. The broker is four
config fields (host, username, password, certificate), so switching is
reversible.

If EMQX's free tier ever goes away, **AWS IoT Core** is the fallback. It
costs a few cents a month for two lamps, but each lamp authenticates with a
device certificate instead of a password, which means more console setup and
a small firmware change. Running our own broker was ruled out: it has to stay
up 24/7 and be reachable from her home, with port forwarding, a domain and
certificate renewals.

## Why not CoronaLamps

It was the leading fallback until the actual guide was read:

- **Step 2 is hand-soldering 12 loose LEDs**, 40+ joints, which the author
  calls "by far the most challenging step". Our whole build is about 10 joints
  per lamp.
- **Known crash:** holding the touch sensor too long resets the lamp. The
  author documents this and never fixed it, and the Arduino forum confirms it.
- **WiFi credentials are hardcoded.** If her router changes, the lamp is dead
  until it comes back to you for reflashing. That alone disqualifies it as a
  gift.
- It skips the data resistor and the power capacitor, and feeds 5 V into a pin
  that isn't 5 V-tolerant: things that work on the bench and fail in month four.
- The guide is from 2020, and the libraries have drifted since.

What's good about it: the STLs, the two-colour print idea, clear beginner
explanations and real soldering photos.

## Other rejected options

| Option | Why not |
|---|---|
| Particle Photon | Discontinued in 2022 |
| Arduino IoT Cloud | Free tier was 2 Things, 25 compiles/day when checked |
| HiveMQ Cloud | The original broker. Its free Serverless tier stopped taking new clusters on 2026-09-30 and shuts down on 2026-12-31. |
| Adafruit IO | Free, but it imposes its own topic layout, so the firmware would need changing |
| HTTP polling | 60 s latency kills the immediacy |
| ESPHome + Home Assistant | Needs HA reachable from her network; overkill |
| Commercial (Filimin, LuvLink) | $150–250 a pair |

## Parked: the cuckoo doors

The original inspiration, from circuitcindy: both people tap within ~10 s, and
servo doors spring open to reveal a photo. It's a meeting, not a notification,
and worth building eventually.

**Why it's parked:** the enclosure has nowhere for it. The diffuser is a
faceted dome (hard to cut clean doors into) and the base is only 34 mm tall
(no room for servos). The STEP files *are* editable, so this is a few hours
of Fusion tutorials rather than "learn CAD", but it most likely needs a new
middle section that matches the 91 mm / 105 mm hex footprint and stacks between
the base and the diffuser. Alternatively, search Printables for "servo door" or
"automata box".

Notes for when it's revived:

- Her mechanism is French doors, one SG90 servo each, with the doors glued
  straight to the servo horns (the horn axis is the hinge). Her angles are
  `OPEN1 2° / CLOSE1 80°` and `OPEN2 178° / CLOSE2 101°`.
- **Call `detach()` after moving.** She didn't, and her servos buzz forever,
  which is a dealbreaker in a bedroom.
- Power the servos from the 5 V rail; each stalls at about 700 mA.
- Firmware side: one more state and two attach/detach calls, a couple of hours.
- **Not until the lamp has passed its 24 h soak.**

## Other ideas, not built

- **Settings over MQTT:** change gesture timing or brightness without a cable.
  About 40 lines.
- **Peer presence:** the `status` topic already reports online/offline; the
  lamp could show a dim indicator when hers is unreachable.
- **OLED screen:** her local time, her last tap, days together and a tap count
  on a small screen in the base. The parts, wiring and printed holder are documented
  ([parts](#optional-oled-screen-not-needed-for-the-lamps),
  [wiring](#optional-oled-screen)). Firmware, about 100–150 lines, isn't
  written yet. Notes for when it is:
  - Use the U8g2 library (handles both SSD1306 and SH1106 screens), on pins
    `D21`/`D22`.
  - Redraw only when something changes (a tap, or the minute ticking over),
    never every frame. A full redraw takes about 25 ms (see rule 1 in
    CLAUDE.md).
  - Record her last tap when a `TAP` arrives in `onMqttMessage()`, after the
    duplicate check, and save tap counts to flash.
  - Her clock needs a time-zone setting for her lamp, and "days together"
    needs a start-date setting; neither exists yet.
  - Shift the text a pixel or two every few minutes: OLEDs burn in static
    text over months.
  - During WiFi setup, show "Join WiFi: Lamp Setup" (WiFiManager's AP
    callback). That helps her at hand-over.
