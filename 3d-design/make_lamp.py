"""
Builds the lamp: a base and diffuser scaled up from Linked Lamp's 66 mm-ring
design to fit a bigger LED ring, so the ring sits in the same proportions and
the glow looks the same. The sizes you'd change are in settings.py.

    cd ~/Projects/lamp_proj
    uv run --no-project --python 3.12 --with cadquery python 3d-design/make_lamp.py

Rebuilds everything in 3d-prints: the print files, the listing pictures, the
listing text and READ ME FIRST.txt (folder names are in paths.py). It never
touches the print profiles you save from Bambu Studio.

What scales and what doesn't: the outside (hex size, heights, ring seat) grows.
Everything that has to fit a real part keeps the original's exact size and
its position relative to the wall it sits against: the ESP32's posts and USB-C
slot, the touch sensor's slot and touch circle, wall and plate thicknesses,
and the 0.1 mm clearance between the diffuser and the base's rim.
"""
import math
import cadquery as cq

import settings
import paths
from screen_holder import add_oled_holder, fit_test
from top_text import top_text
from make_pictures import render_all

# Original Linked Lamp dimensions, measured from its STEP files.
F0 = 90.6             # across the flats
BASE_H0 = 33.78       # base height, bottom plate included
DIFF_H0 = 77.2        # diffuser height, top plate included
CAVITY_Y0 = 25.0      # the base's hollow spans |y| < this
LOGO_SRC = paths.ORIG_DIFFUSER

# Kept at the original's size.
BOTTOM = 2.54         # base bottom plate
WALL = 2.0            # base walls at the two flat ends (USB and touch sides)
LIP_H = 3.0                 # rim the diffuser slides over
# The rim is set in by the diffuser wall plus 0.1 mm clearance, so the diffuser
# sits flush with the base. Two bases are already printed with this rim: never
# change LIP_IN or DIFF_WALL, or new diffusers won't fit them.
LIP_IN = 0.5
SEAT_DEPTH = 1.6
NOTCH_W, NOTCH_OUT = 3.82, 2.59   # wire notch at each end of the ring seat
# 0.4 mm is one printed line, as in the original. Bambu's standard "classic"
# wall generator drops walls this thin and prints only the top plate, so the
# diffuser's print profile must use the Arachne wall generator. Thickening the
# wall doesn't help: classic prints nothing under ~0.44 mm, two overlapping
# lines up to ~0.84 mm, and a flush 0.84 mm wall wouldn't fit over the rim.
DIFF_WALL, DIFF_PLATE = 0.4, 1.0
LOGO_DEPTH = 0.4

SCALE = settings.SCALE
F = F0 * SCALE
R_FLAT = F / 2
TOP = -BOTTOM + BASE_H0 * SCALE          # base top, with the hollow's floor at z=0
SEAT_R = (settings.RING_OD_MAX + settings.SEAT_CLEAR) / 2
CAVITY_Y = CAVITY_Y0 * SCALE
INNER_X = R_FLAT - WALL                  # inside face of the flat end walls
assert SEAT_R + 8 < R_FLAT - LIP_IN, "ring seat too close to the rim"
assert CAVITY_Y + 8 < SEAT_R, "seat needs a ledge beyond the hollow"


def hex_prism(across_flats, z0, z1):
    # Flats face ±X, corners point ±Y, matching the original files.
    return (cq.Workplane("XY").workplane(offset=z0)
            .polygon(6, across_flats / math.cos(math.pi / 6)).extrude(z1 - z0)
            .rotate((0, 0, 0), (0, 0, 1), 30))


# ---------------------------------------------------------------- base
base = (hex_prism(F, -BOTTOM, TOP - LIP_H)
        .union(hex_prism(F - 2 * LIP_IN, TOP - LIP_H, TOP)))
base = base.cut(cq.Workplane("XY").box(2 * INNER_X, 2 * CAVITY_Y, TOP + 1, centered=(True, True, False)))

# Ring seat: a round pocket at the top, with a wire notch at each end.
base = base.cut(cq.Workplane("XY").workplane(offset=TOP - SEAT_DEPTH).circle(SEAT_R).extrude(SEAT_DEPTH + 1))
notch_len = SEAT_R + NOTCH_OUT
for y0 in (0, -notch_len):
    base = base.cut(cq.Workplane("XY").box(NOTCH_W, notch_len, SEAT_DEPTH + 1, centered=(True, False, False))
                    .translate((0, y0, TOP - SEAT_DEPTH)))

# ESP32: four posts (3.18 mm, 5.33 tall) with 2.03 mm locating pins on top,
# 3.36 and 50.35 mm in from the USB-side wall, ±11.43 mm either side.
for px in (INNER_X - 3.36, INNER_X - 50.35):
    for py in (11.43, -11.43):
        post = (cq.Workplane("XY").center(px, py).circle(3.18 / 2).extrude(5.33)
                .union(cq.Workplane("XY").workplane(offset=5.33).center(px, py)
                       .circle(2.03 / 2).extrude(10.16 - 5.33)))
        base = base.union(post)

# USB-C slot through the +X wall, 8.66 x 3.56 with 0.25 mm corners.
usb = (cq.Workplane("YZ").workplane(offset=INNER_X - 0.5).center(0, 2.03 + 3.56 / 2)
       .rect(8.66, 3.56).extrude(WALL + 1).edges("|X").fillet(0.25))
base = base.cut(usb)

# Touch sensor on the -X wall: a slot open at the top, made of a 0.9 mm recess
# in the wall plus a 2 mm pocket in a holder block; and a 0.2 mm-deep circle
# on the outside marking where to touch.
recess = cq.Workplane("XY").box(0.9, 11.3, TOP - 3.35 + 1, centered=(False, True, False)).translate((-INNER_X - 0.9, 0, 3.35))
holder = cq.Workplane("XY").box(4.54, 13.98, 14.3, centered=(False, True, False)).translate((-INNER_X, 0, 0))
pocket = cq.Workplane("XY").box(2.0, 11.42, 14.3 - 3.3 + 1, centered=(False, True, False)).translate((-INNER_X, 0, 3.3))
circle = cq.Workplane("YZ").workplane(offset=-R_FLAT - 0.1).center(0, 9.27).circle(7.36 / 2).extrude(0.3)
base = base.cut(recess).union(holder).cut(pocket).cut(circle)

# ---------------------------------------------------------------- diffuser
# Built as printed: the top plate on the bed at z = -1..0, the open end up.
diff_h = DIFF_H0 * SCALE
diffuser = (hex_prism(F, -DIFF_PLATE, diff_h - DIFF_PLATE)
            .cut(hex_prism(F - 2 * DIFF_WALL, 0, diff_h)))
# Engraved 0.4 mm into the top: the text from settings.py, or Linked Lamp's
# logo scaled with the lamp.
if settings.TOP_TEXT:
    logo = top_text(settings.TOP_TEXT, settings.TOP_TEXT_WIDTH, settings.TOP_HEARTS,
                    depth=LOGO_DEPTH, face_z=-DIFF_PLATE)
    # Keep it 5 mm inside the wall on every side of the hexagon (flats face ±X).
    apothem = R_FLAT - DIFF_WALL - 5
    lb = logo.val().BoundingBox()
    for x in (lb.xmin, lb.xmax):
        for y in (lb.ymin, lb.ymax):
            assert abs(x) <= apothem and abs(x) / 2 + abs(y) * math.sqrt(3) / 2 <= apothem, \
                "TOP_TEXT is too close to the edge: lower TOP_TEXT_WIDTH in settings.py"
else:
    orig = cq.importers.importStep(str(LOGO_SRC))
    logo_zone = cq.Workplane("XY").box(40, 46, LOGO_DEPTH, centered=(True, True, False)).translate((1.2, 0.8, -DIFF_PLATE))
    logo = logo_zone.cut(orig)                   # the engraved pockets, as solids
    logo = cq.Workplane("XY").add(logo.val().transformGeometry(
        cq.Matrix([[SCALE, 0, 0, 0], [0, SCALE, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]])))
diffuser = diffuser.cut(logo)

# ---------------------------------------------------------------- outputs
def save(wp, folder, name):
    folder.mkdir(parents=True, exist_ok=True)
    cq.exporters.export(wp, str(folder / f"{name}.stl"), tolerance=0.01, angularTolerance=0.1)

save(base.intersect(cq.Workplane("XY").box(400, 400, 5, centered=(True, True, False)).translate((0, 0, TOP - 5))),
     paths.PRINT, paths.FIT_RIM)
save(diffuser, paths.PRINT, paths.DIFFUSER)
save(base, paths.PRINT, paths.BASE)
paths.PROFILES.mkdir(parents=True, exist_ok=True)

if settings.MAKE_SCREEN_VERSION:
    # Slot floor set so the picture's centre sits mid-way up the visible face
    # (bottom of the base to the bottom of the diffuser).
    visible_mid = (-BOTTOM + TOP - LIP_H) / 2
    slot_floor = round(visible_mid - 16.0, 1)
    oled, plane, _, _ = add_oled_holder(base, slot_floor=slot_floor, cavity_half_y=CAVITY_Y)
    save(oled, paths.SCREEN, paths.SCREEN_BASE)
    save(fit_test(oled, plane, slot_floor, top_z=TOP, bottom_z=-BOTTOM), paths.SCREEN, paths.SCREEN_FIT)
elif paths.SCREEN.exists():
    # Turned off since the last build: remove the old screen files so nobody
    # prints a base that doesn't match settings.py.
    for name in (paths.SCREEN_BASE, paths.SCREEN_FIT):
        (paths.SCREEN / f"{name}.stl").unlink(missing_ok=True)
    if not any(paths.SCREEN.iterdir()):
        paths.SCREEN.rmdir()

# Measured before rendering: rendering meshes the shapes, which pads their
# bounding boxes by a few tenths of a mm.
summary = []
for name, wp in (("base", base), ("diffuser", diffuser)):
    bb = wp.val().BoundingBox()
    summary.append(f"{name:9} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:6.1f} mm  "
                   f"volume {wp.val().Volume()/1000:6.1f} cm3  solids={len(wp.solids().vals())}  valid={wp.val().isValid()}")

render_all(base, diffuser, top=TOP, diff_h=diff_h, lip_h=LIP_H, ring_od=settings.RING_OD_MAX,
           ring_id=settings.RING_ID, leds=settings.RING_LEDS, scale=SCALE)

# ---------------------------------------------------------------- text files
ring = f"{settings.RING_OD_MIN:g}-{settings.RING_OD_MAX:g} mm" if settings.RING_OD_MIN != settings.RING_OD_MAX else f"{settings.RING_OD_MAX:g} mm"
across = F
height = TOP - LIP_H + diff_h + BOTTOM
seat = 2 * SEAT_R
screen_note = ("""
3 Optional - base with screen window
    Only if you're adding the OLED screen. Print this base instead of 3, and
    its fit test first.
""" if settings.MAKE_SCREEN_VERSION else "")
paths.README.write_text(f"""3D files for the friendship lamps ({ring}, {settings.RING_LEDS}-LED ring)

Made by 3d-design/make_lamp.py. Don't edit these files: change
3d-design/settings.py and run the script again (see 3d-design/HOW TO CHANGE
THE LAMP.txt).

1 Print these
    Everything that gets printed, in print order:
      1  Fit test: the top of the base. Print first, in any colour. The LED
         ring should drop into its round seat.
      2  Diffuser: white PLA, print 2 (one per lamp). Its walls are one
         0.4 mm line, so its print profile must set Wall generator to
         Arachne: with Bambu's default (Classic) only the top plate prints.
         Print one first and check it slides over the fit-test piece before
         printing the second.
      3  Base: dark PLA, print 2

2 MakerWorld upload kit
    Listing text to copy into MakerWorld, pictures for the listing, and a
    folder to save the Bambu Studio print profiles into. The steps are in the
    online build guide, Phase F.
{screen_note}
Size: {across:.0f} mm across, {height:.0f} mm tall assembled. Ring seat {seat:.1f} mm.
""")

top_note = (f'The top is engraved with "{settings.TOP_TEXT}" in a script font'
            f'{" with hearts" if settings.TOP_HEARTS else ""}; it glows brighter than the rest when lit.'
            if settings.TOP_TEXT else "The top has the original's logo, engraved.")
paths.LISTING.write_text(f"""# MakerWorld listing: copy and paste

Made by 3d-design/make_lamp.py, with the current sizes filled in.

Upload these with the listing:
- Model files: the three `.stl` files in `1 Print these`
- Pictures: the four renders in `pictures` (add real photos later)
- Print profiles: the three `.3mf` files you save from Bambu Studio into
  `print profiles - save Bambu Studio files here` (Fit test, Diffuser, Base)

---

**Title**

Friendship Lamp for {ring} {settings.RING_LEDS}-LED rings (Linked Lamp remix)

**Category**

Home & Living → Lighting (or the closest lighting category)

**License**

CC BY-SA 4.0 — the same as the original, which its license requires.

**Original or remix**

Remix of Linked Lamp by Aarush Magic:
https://makerworld.com/en/models/2886665-linked-lamp-esp32-friendship-lamps

**Description**

A scaled-up version of Aarush Magic's Linked Lamp for WS2812B rings with
{settings.RING_LEDS} LEDs and a {ring} outside diameter, which are too big for the
original's 69 mm ring seat.

The base and diffuser are {SCALE:.2f}× larger so the ring sits in the same
proportions and the glow looks the same: about {across:.0f} mm across and
{height:.0f} mm tall assembled. Everything that has to fit a real part keeps
the original's size and position: the ESP32 posts and USB-C slot, the touch
sensor slot and touch circle, and the fit between the diffuser and the base.
The ring seat is {seat:.1f} mm across. {top_note}

What to print, per lamp:
- Base: dark PLA
- Diffuser: white PLA. It prints top-down, with the engraved top face on the bed.

Print the fit test first: the top 5 mm of the base, in any colour. The ring
should drop into its seat, and the diffuser should slide over its rim.

Settings: 0.2 mm layers, 2 walls, 15% infill, no supports. For the diffuser,
set Wall generator to Arachne: its walls are a single 0.4 mm line, as in the
original, and Bambu's default (Classic) skips them and prints only the top
plate. The included print profiles already have this set.

Hardware: 30-pin ESP32 DevKit with USB-C, a TTP223 touch sensor, and a
{settings.RING_LEDS}-LED WS2812B ring with a {ring} outside diameter. With
{settings.RING_LEDS} LEDs on USB power, keep the LED brightness at about half of
maximum.

All credit for the original design goes to Aarush Magic.
""")

print(*summary, sep="\n")
print(f"ring seat {seat:.1f} mm for a {ring} ring; lamp {across:.0f} mm across, {height:.0f} mm tall; "
      f"screen version {'made' if settings.MAKE_SCREEN_VERSION else 'off'}")
