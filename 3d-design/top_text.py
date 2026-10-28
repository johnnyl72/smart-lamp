"""
The text engraved into the diffuser's top: a word in Pacifico, with a heart
dotting each i and one either side. make_lamp.py calls top_text().

The font is Pacifico (fonts/Pacifico-Regular.ttf, SIL Open Font License in
fonts/OFL.txt): a bold script whose thinnest strokes stay wider than the
0.8 mm (two printed lines) a 0.4 mm nozzle needs. Thin elegant scripts lose
their hairlines when printed.
"""
import math
from pathlib import Path
import cadquery as cq

FONT = Path(__file__).resolve().parent / "fonts" / "Pacifico-Regular.ttf"


def _heart(height, cx, cy, tilt_deg, thick):
    # The classic heart curve, scaled to `height` and centred on its bounding box.
    pts = []
    for i in range(240):
        t = 2 * math.pi * i / 240
        pts.append((16 * math.sin(t) ** 3,
                    13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)))
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    k = height / (max(ys) - min(ys))
    mx, my = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
    a = math.radians(tilt_deg)
    out = []
    for x, y in pts:
        x, y = (x - mx) * k, (y - my) * k
        out.append((cx + x * math.cos(a) - y * math.sin(a), cy + x * math.sin(a) + y * math.cos(a)))
    return cq.Workplane("XY").polyline(out).close().extrude(thick).val()


def top_text(text, width, hearts, depth, face_z):
    """The engraved pockets as solids, placed for the diffuser as printed:
    the top face on the bed at z = face_z, so they reach depth mm up from it.

    Drawn as read from above the finished lamp, then mirrored: the top prints
    face down, like the letters on a stamp. Reads left to right along a flat
    of the hexagon, so turning the diffuser on the base turns the text to
    face whichever side you like.
    """
    thick = depth + 0.1                          # starts 0.1 mm below the face, for a clean cut
    probe = cq.Workplane("XY").text(text, 20, thick, fontPath=str(FONT), halign="center", valign="center")
    size = 20 * width / probe.val().BoundingBox().xlen
    letters = cq.Workplane("XY").text(text, size, thick, fontPath=str(FONT), halign="center",
                                      valign="center", combine=False).solids().vals()
    pieces = list(letters)

    if hearts:
        # The dot of an i or j is a small piece of its own, above the others.
        areas = sorted(s.Volume() for s in letters)
        median_area = areas[len(areas) // 2]
        median_cy = sorted(s.Center().y for s in letters)[len(letters) // 2]
        dots = [s for s in letters if s.Volume() < 0.2 * median_area and s.Center().y > median_cy]
        for d in dots:
            b = d.BoundingBox()
            pieces.remove(d)
            # Bigger than the dot so it reads as a heart; nudged up so its point
            # clears the stem.
            pieces.append(_heart(1.9 * b.ylen, b.center.x, b.center.y + 0.35 * b.ylen, 0, thick))
        body = cq.Compound.makeCompound(pieces).BoundingBox()
        # Baseline and x-height from the lowercase letters, which outnumber the
        # capitals and tails.
        bottoms = sorted(s.BoundingBox().ymin for s in letters)
        tops = sorted(s.BoundingBox().ymax for s in letters if s not in dots)
        baseline = bottoms[len(bottoms) // 2]
        xh = tops[len(tops) // 2] - baseline
        h, gap, cy = 1.05 * xh, 0.3 * xh, baseline + 0.5 * xh
        pieces.append(_heart(h, body.xmin - gap - 0.55 * h, cy, 15, thick))
        pieces.append(_heart(h, body.xmax + gap + 0.55 * h, cy, -15, thick))

    # Fused into one shape first: joined-up letters overlap, and cutting the
    # diffuser with overlapping pieces fails silently, leaving a scrap.
    # Then mirror, and turn so the reading direction runs along the hexagon's
    # flats (flats face ±X, so along Y); then drop onto the bed face.
    shape = (pieces[0].fuse(*pieces[1:]).clean()
             .mirror("XZ").rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), 90))
    return cq.Workplane("XY").add(shape.translate(cq.Vector(0, 0, face_z - 0.1)))
