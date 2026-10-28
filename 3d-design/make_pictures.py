"""
Renders the MakerWorld listing pictures. make_lamp.py calls render_all()
after building the parts.
"""
import math
import cadquery as cq
import vtk

import paths

DARK, WHITE, PINK, RING = (0.16, 0.17, 0.2), (0.97, 0.97, 0.95), (1.0, 0.4, 0.8), (0.1, 0.1, 0.12)


def _actor(shape, rgb, opacity=1.0, specular=0.2, fine=False):
    # Fine meshing keeps the curves of the engraved text smooth.
    poly = shape.val().toVtkPolyData(*((0.01, 0.05) if fine else (0.05, 0.2)))
    normals = vtk.vtkPolyDataNormals(); normals.SetInputData(poly)
    normals.ConsistencyOn(); normals.AutoOrientNormalsOn(); normals.SplittingOn(); normals.SetFeatureAngle(30)
    m = vtk.vtkPolyDataMapper(); m.SetInputConnection(normals.GetOutputPort())
    a = vtk.vtkActor(); a.SetMapper(m)
    p = a.GetProperty(); p.SetColor(*rgb); p.SetOpacity(opacity); p.SetSpecular(specular); p.SetSpecularPower(30)
    p.SetAmbient(0.25); p.SetDiffuse(0.8)
    return a


def _render(actors, name, cam_pos, focal, w=1600, h=1200, bg=(0.93, 0.94, 0.96)):
    r = vtk.vtkRenderer(); r.SetBackground(*bg)
    for a in actors:
        r.AddActor(a)
    r.SetUseDepthPeeling(1); r.SetMaximumNumberOfPeels(20)
    cam = r.GetActiveCamera(); cam.SetPosition(*cam_pos); cam.SetFocalPoint(*focal); cam.SetViewUp(0, 0, 1)
    r.ResetCameraClippingRange()
    kit = vtk.vtkLightKit(); kit.SetKeyLightIntensity(0.9); kit.AddLightsToRenderer(r)
    r.SetTwoSidedLighting(1)
    win = vtk.vtkRenderWindow(); win.SetOffScreenRendering(1); win.SetSize(w, h); win.SetAlphaBitPlanes(1)
    win.AddRenderer(r); win.Render()
    grab = vtk.vtkWindowToImageFilter(); grab.SetInput(win); grab.Update()
    png = vtk.vtkPNGWriter(); png.SetFileName(str(paths.PICTURES / name))
    png.SetInputConnection(grab.GetOutputPort()); png.Write()


def render_all(base, diffuser, top, diff_h, lip_h, ring_od, ring_id, leds, scale):
    """base and diffuser as built (diffuser top-down, as printed)."""
    paths.PICTURES.mkdir(parents=True, exist_ok=True)
    # Assembled: diffuser flipped (top face up) and seated on the base's rim.
    diff_up = diffuser.rotate((0, 0, 0), (1, 0, 0), 180).translate((0, 0, top - lip_h + diff_h - 1))
    ring = cq.Workplane("XY").circle(ring_od / 2).circle(ring_id / 2).extrude(1.6).translate((0, 0, top - 1.6))
    led_r = (ring_od + ring_id) / 4
    led_actors = [_actor(cq.Workplane("XY").box(5, 5, 1.6)
                         .translate((led_r * math.cos(a), led_r * math.sin(a), top + 0.8)), PINK, 1.0, 0.0)
                  for a in (2 * math.pi * i / leds for i in range(leds))]
    s = scale / (87 / 66)   # camera distances were set for the 87 mm-ring lamp
    _render([_actor(base, DARK), _actor(ring, RING), *led_actors, _actor(diff_up, WHITE, 0.55)],
            "1 assembled.png", (-260 * s, -330 * s, 230 * s), (0, 0, 70 * s))
    _render([_actor(base, DARK), _actor(ring, RING), *led_actors,
             _actor(diff_up.translate((0, 0, 70 * s)), WHITE, 0.9)],
            "2 exploded.png", (-280 * s, -360 * s, 260 * s), (0, 0, 95 * s))
    _render([_actor(base, DARK)], "3 base from above.png", (-120 * s, -170 * s, 260 * s), (0, 0, 15 * s))
    # From the -X side, where the top's text reads the right way up.
    (paths.PICTURES / "4 diffuser top with logo.png").unlink(missing_ok=True)   # the old name
    _render([_actor(diff_up, WHITE, fine=True)], "4 diffuser top.png", (-150 * s, 0, 430 * s), (0, 0, 110 * s))
