"""
The lamp's settings: the only values you'd normally change.

After changing one, save this file and rebuild everything in 3d-prints. Paste
into Terminal and press Return:

    cd ~/Projects/lamp_proj
    uv run --no-project --python 3.12 --with cadquery python 3d-design/make_lamp.py
"""

# Your LED ring's outside diameter in mm: the smallest and largest readings
# you measured. The ring seat is made RING_OD_MAX + SEAT_CLEAR across, so the
# biggest ring drops in and the smallest still sits close to the centre. More
# clearance lets a small ring slide off-centre, and the glow looks lopsided.
RING_OD_MIN = 85.0
RING_OD_MAX = 87.0
SEAT_CLEAR = 1.0

# The ring's inside diameter and LED count. These only appear in the pictures
# and the listing text. The firmware's LED count is RING_PIXELS in src/config.h.
RING_ID = 70.5
RING_LEDS = 24

# How much bigger than Linked Lamp's original, which is built around a 66 mm
# ring. Scaling with the ring keeps it sitting in the same proportions, so the
# glow looks the same.
SCALE = RING_OD_MAX / 66.0

# The text engraved into the diffuser's top, in a bold script font
# (Pacifico), with a heart dotting each i and one either side. Engraved parts
# are thinner, so they glow brighter when the lamp is lit. TOP_TEXT_WIDTH is
# the word alone in mm; the side hearts add about a quarter of it each side.
# Around 70 mm suits a 6-letter word; the build stops with a message if the
# design gets too close to the edge. Set TOP_TEXT = "" to engrave Linked
# Lamp's original logo instead, and TOP_HEARTS = False for just the word.
TOP_TEXT = "Pookie"
TOP_TEXT_WIDTH = 70
TOP_HEARTS = True

# Also make a base with a window and holder for a 0.96" OLED screen.
# True or False.
MAKE_SCREEN_VERSION = False
