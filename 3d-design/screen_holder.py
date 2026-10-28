"""
Adds a printed window and slide-in holder for a 0.96" SSD1306 OLED module to
the lamp's base, so the screen fits without cutting the print by hand.
make_lamp.py uses it when MAKE_SCREEN_VERSION is True in settings.py.

How it works: the module goes in from the hollow inside the base, glass facing
the window, and is pushed forward into a slot behind a 2 mm wall. The slot's
floor is 0.8 mm lower than the passage behind it, so the module drops into
the slot and can't slide back out unless you lift it. No glue.

Module dimensions come from the common 4-pin module drawing (PCB 27.3 x 27.8,
glass 26.7 x 19.27 starting 4.27 below the pin edge, active area 21.74 x 10.86
starting 2.10 below the glass top, glass + PCB about 2.7 thick). The module
sits pins-up, so the picture is the right way up with no firmware rotation.
"""
import math
import cadquery as cq

# Which angled face gets the screen, as its outward direction in the XY plane.
# (-0.5, -0.866) is the face next to the touch side (-X). Any of the four
# angled faces works the same: (±0.5, ±0.866).
FACE = (-0.5, -math.sqrt(3) / 2)

WALL = 2.0            # plastic left in front of the glass, behind the window
WIN_W, WIN_H = 24.0, 13.0   # window; active area is 21.74 x 10.86, so ~1 mm of
                            # black glass shows round the picture
BEVEL = 0.8           # 45° chamfer round the window's outside edge
SLOT_W = 28.0         # module is 27.3 wide
SLOT_D = 3.2          # glass + PCB is ~2.7 thick
DROP = 0.8            # how far the module drops into the slot: the lock
MODULE_H = 27.8
AA_FROM_BOTTOM = 27.8 - (4.27 + 2.10 + 10.86 / 2)   # active-area centre, pins up


def add_oled_holder(base, slot_floor, cavity_half_y, face=FACE):
    """Cut the window, slot and passage into `base` behind the given face.

    slot_floor: z of the slot's floor. Below 0 digs into the bottom plate,
        which the original base needs so the passage roof clears the ring seat.
    cavity_half_y: the hollow spans |y| < this; the part of the passage below
        the hollow's floor is kept outside it so the floor isn't grooved.
    Returns (result, plane, face_centre) so callers can cut a fit-test piece.
    """
    fx, fy = face
    f = max((f for f in base.faces().vals()
             if f.geomType() == "PLANE"
             and abs(f.normalAt().x - fx) < 1e-3 and abs(f.normalAt().y - fy) < 1e-3),
            key=lambda f: f.Area())
    c = f.Center()
    n_in = cq.Vector(-fx, -fy, 0)
    along = cq.Vector(fy, -fx, 0)               # chosen so local Y = n_in x along = +Z
    plane = cq.Plane(origin=(c.x, c.y, 0), xDir=along, normal=n_in)
    assert abs(plane.yDir.z - 1) < 1e-6, "local Y must be world Z"

    def block(u0, u1, z0, z1, w0, w1):
        """Box in face coordinates: u along the face, z height, w depth inward."""
        return (cq.Workplane(plane).center((u0 + u1) / 2, (z0 + z1) / 2)
                .rect(u1 - u0, z1 - z0).extrude(w1 - w0).translate(n_in * w0))

    zc = slot_floor + AA_FROM_BOTTOM
    slot_top = slot_floor + MODULE_H + 0.4
    pass_floor = slot_floor + DROP
    pass_top = pass_floor + MODULE_H + 0.6

    window = block(-WIN_W / 2, WIN_W / 2, zc - WIN_H / 2, zc + WIN_H / 2, -1, WALL + 0.2)
    bevel = (cq.Workplane(plane).center(0, zc)
             .rect(WIN_W + 2 * (BEVEL + 0.2), WIN_H + 2 * (BEVEL + 0.2))
             .workplane(offset=BEVEL + 0.2).rect(WIN_W, WIN_H).loft()
             .translate(n_in * -0.2))
    slot = block(-SLOT_W / 2, SLOT_W / 2, slot_floor, slot_top, WALL, WALL + SLOT_D)
    result = base.cut(window).cut(bevel).cut(slot)

    # Passage from the slot back into the hollow. It's only cut outside the
    # hollow: inside is open air already, and a longer cut would clip the
    # ESP32's posts and groove the hollow's floor.
    passage = block(-SLOT_W / 2, SLOT_W / 2, pass_floor, pass_top,
                    WALL + SLOT_D - 0.01, WALL + SLOT_D + 40)
    outside = cq.Workplane("XY").box(400, 200, 400).translate(
        (0, math.copysign(cavity_half_y + 100, fy), 0))
    result = result.cut(passage.intersect(outside))
    return result, plane, c, zc


def fit_test(result, plane, slot_floor, top_z, bottom_z):
    """Full-height slice of the base around the holder, for a short test print."""
    h = top_z - bottom_z + 0.4
    box = (cq.Workplane(plane).center(0, (top_z + bottom_z) / 2).rect(SLOT_W + 8, h)
           .extrude(13).translate(plane.zDir * -1))
    return result.intersect(box)
