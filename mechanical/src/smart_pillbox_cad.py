"""
Smart pillbox CAD model.
Units: millimeters.
Coordinate system: origin at enclosure footprint center; X width left/right, Y depth front/back, Z up.

This file is generated for the smart pillbox course-design enclosure.
It exposes gen_step() for the full placeholder assembly.  Additional helper
functions are used by small wrapper files to export pure printable assembly
and individual printable parts.
"""
from __future__ import annotations

import inspect
import math
import platform

# Work around a build123d Windows font scan issue caused by malformed system fonts.
_REAL_PLATFORM_SYSTEM = platform.system

def _platform_system_patch():
    for fr in inspect.stack()[:10]:
        if fr.filename.replace("\\", "/").endswith("build123d/text.py"):
            return "Linux"
    return _REAL_PLATFORM_SYSTEM()

platform.system = _platform_system_patch

from build123d import *  # noqa: E402,F403
from cadpy.assembly import AssemblyHelper, label_shape  # noqa: E402

# -------------------------- Parameters --------------------------
OUTER_X = 165.0
OUTER_Y = 200.0
OUTER_Z = 115.0
WALL = 2.5
CLEARANCE = 0.4
CORNER_R = 10.0

BASE_H = 92.0
LID_H = 24.0
LID_SKIRT_H = 10.0
LID_TOP_TH = 3.0
LID_OVERLAP = 0.4

FRONT_Y = -OUTER_Y / 2
BACK_Y = OUTER_Y / 2

# QR100, based on the provided manual.
QR_FACE_W = 65.0
QR_FACE_H = 53.0
QR_FACE_TH = 8.3
QR_BODY_W = 54.3
QR_BODY_H = 31.8
QR_BODY_DEPTH = 65.3
QR_RECESS_CLEAR = 0.6
QR_BODY_CLEAR = 0.8
QR_CENTER_Z = 34.0

# Dispense window.
DISP_W = 70.0
DISP_H = 35.0
DISP_CENTER_Z = 72.0

# Cable hole.
CABLE_D = 25.0
CABLE_CENTER_Z = 35.0

# Scale / motor / turntable stack.
SCALE_D = 100.0
SCALE_TH = 3.5
SCALE_Z = 9.0
ISLAND_D = 82.0
ISLAND_H = 7.0
ISLAND_Z = SCALE_Z + SCALE_TH / 2 + ISLAND_H / 2 + 0.8
MOTOR_X = 42.3
MOTOR_BODY_H = 48.0
MOTOR_SLOT = 43.5
MOTOR_Z = ISLAND_Z + ISLAND_H / 2 + MOTOR_BODY_H / 2
MOTOR_SHAFT_D = 5.0
MOTOR_SHAFT_H = 22.0
SHAFT_Z = ISLAND_Z + ISLAND_H / 2 + MOTOR_BODY_H + MOTOR_SHAFT_H / 2
TURNTABLE_D = 110.0
TURNTABLE_H = 29.0
TRAY_FLOOR = 4.0
TRAY_WALL = 2.0
TRAY_Z = ISLAND_Z + ISLAND_H / 2 + MOTOR_BODY_H + 7.0 + TURNTABLE_H / 2

# Rear electronics bay.
E_BAY_W = 130.0
E_BAY_D = 42.0
E_BAY_H = 32.0
E_BAY_CENTER_Y = 58.0
E_BAY_CENTER_Z = 27.0

# Visualization colors.
COL_BASE = Color(0.84, 0.86, 0.88, 1.0)
COL_LID = Color(0.92, 0.93, 0.95, 1.0)
COL_TRAY = Color(0.95, 0.95, 0.88, 1.0)
COL_SCALE = Color(0.55, 0.75, 0.95, 0.55)
COL_MOTOR = Color(0.1, 0.1, 0.1, 0.7)
COL_METAL = Color(0.75, 0.75, 0.75, 0.85)
COL_QR = Color(0.05, 0.05, 0.05, 0.85)
COL_BOARD = Color(0.05, 0.55, 0.25, 0.65)


def _rounded_box(x: float, y: float, z: float, r: float, label: str | None = None, center=(0, 0, 0)):
    """Create a rounded rectangular solid with coordinates baked into geometry."""
    with BuildPart() as part:
        with Locations(center):
            Box(x, y, z)
        edges = part.edges().filter_by(Axis.Z)
        if r > 0:
            fillet(edges, radius=min(r, x / 2 - 0.2, y / 2 - 0.2))
    obj = part.part
    if label:
        obj.label = label
    return obj


def _translated(shape, x=0, y=0, z=0):
    # build123d shapes compose reliably with Location multiplication.
    return Location((x, y, z)) * shape
def _base_solid(include_cutouts: bool = True):
    """Bottom enclosure with open top and front/back cutouts."""
    zc = BASE_H / 2
    with BuildPart() as p:
        # main rounded shell block
        add(_translated(_rounded_box(OUTER_X, OUTER_Y, BASE_H, CORNER_R), z=zc))
        # hollow top cavity, leaving side walls and bottom floor
        add(_translated(Box(OUTER_X - 2 * WALL, OUTER_Y - 2 * WALL, BASE_H - WALL + 2), z=WALL + (BASE_H - WALL + 2) / 2), mode=Mode.SUBTRACT)
        # front dispense opening
        add(_translated(Box(DISP_W, WALL + 6, DISP_H), y=FRONT_Y, z=DISP_CENTER_Z), mode=Mode.SUBTRACT)
        # QR face shallow step recess on front lower panel
        add(_translated(Box(QR_FACE_W + QR_RECESS_CLEAR, WALL + 1.2, QR_FACE_H + QR_RECESS_CLEAR), y=FRONT_Y - 0.1, z=QR_CENTER_Z), mode=Mode.SUBTRACT)
        # QR smaller through/body clearance into enclosure
        add(_translated(Box(QR_BODY_W + QR_BODY_CLEAR, QR_BODY_DEPTH + 5, QR_BODY_H + QR_BODY_CLEAR), y=FRONT_Y + QR_BODY_DEPTH / 2, z=QR_CENTER_Z), mode=Mode.SUBTRACT)
        # rear cable hole, horizontal cylinder through back wall
        add(_translated(Cylinder(CABLE_D / 2, WALL + 8, rotation=(90, 0, 0)), y=BACK_Y, z=CABLE_CENTER_Z), mode=Mode.SUBTRACT)
        # rear electronics bay recessed floor/platform pocket, not through wall
        add(_translated(Box(E_BAY_W + 2, E_BAY_D + 2, E_BAY_H), y=E_BAY_CENTER_Y, z=E_BAY_CENTER_Z), mode=Mode.SUBTRACT)
        # scale recess pocket in bottom interior
        add(_translated(Cylinder(SCALE_D / 2 + 2, SCALE_TH + 1), z=SCALE_Z), mode=Mode.SUBTRACT)
        # add a front lip around QR to show outer frame support
        add(_translated(_rounded_box(QR_FACE_W + 7, 3.0, QR_FACE_H + 7, 3.0), y=FRONT_Y - 1.0, z=QR_CENTER_Z))
        add(_translated(Box(QR_FACE_W + QR_RECESS_CLEAR, 5.0, QR_FACE_H + QR_RECESS_CLEAR), y=FRONT_Y - 1.2, z=QR_CENTER_Z), mode=Mode.SUBTRACT)
        # add thin shelf under dispense opening for visual support
        add(_translated(_rounded_box(DISP_W + 12, 7.0, 3.0, 2.0), y=FRONT_Y + 3.0, z=DISP_CENTER_Z - DISP_H / 2 - 3.0))
    obj = p.part
    obj.label = "base_print_part"
    obj.color = COL_BASE
    return obj


def _lid_solid():
    """Friction-fit lid with perforation array over tray area."""
    lid_center_z = BASE_H + LID_H / 2 - 1.0
    with BuildPart() as p:
        add(_translated(_rounded_box(OUTER_X + LID_OVERLAP, OUTER_Y + LID_OVERLAP, LID_H, CORNER_R), z=lid_center_z))
        # inner hollow/skirt clearance from bottom, leaves top plate
        inner_h = LID_H - LID_TOP_TH
        add(_translated(_rounded_box(OUTER_X - 2 * WALL + 0.8, OUTER_Y - 2 * WALL + 0.8, inner_h + 1.0, CORNER_R - WALL), z=BASE_H - 1 + inner_h / 2), mode=Mode.SUBTRACT)
        # bottom opening cleanup
        add(_translated(Box(OUTER_X - 4, OUTER_Y - 4, 4), z=BASE_H - 3), mode=Mode.SUBTRACT)
        # perforated observation/vent holes over turntable circular region
        hole_d = 5.0
        for r in [0, 13, 26, 39, 50]:
            count = 1 if r == 0 else max(6, int(2 * math.pi * r / 16))
            for i in range(count):
                ang = 0 if count == 1 else 2 * math.pi * i / count
                x = r * math.cos(ang)
                y = r * math.sin(ang)
                if x * x + y * y <= 52 ** 2:
                    add(_translated(Cylinder(hole_d / 2, LID_TOP_TH + 4), x=x, y=y, z=BASE_H + LID_H - LID_TOP_TH / 2), mode=Mode.SUBTRACT)
        # front clearance notch above dispense zone so lid doesn't block hand access
        add(_translated(Box(DISP_W + 8, 10, 18), y=FRONT_Y + 2, z=BASE_H + 2), mode=Mode.SUBTRACT)
    obj = p.part
    obj.label = "lid_print_part"
    obj.color = COL_LID
    return obj


def _turntable_solid():
    """110 mm diameter three-sector tray with D-shaped motor shaft hole."""
    with BuildPart() as p:
        # disk floor
        Cylinder(TURNTABLE_D / 2, TRAY_FLOOR)
        # outer rim
        add(Cylinder(TURNTABLE_D / 2, TURNTABLE_H), mode=Mode.ADD)
        add(Cylinder(TURNTABLE_D / 2 - TRAY_WALL, TURNTABLE_H + 1), mode=Mode.SUBTRACT)
        # restore floor after shell subtraction
        add(Cylinder(TURNTABLE_D / 2 - TRAY_WALL, TRAY_FLOOR), mode=Mode.ADD)
        # three radial divider walls
        for ang in [0, 120, 240]:
            divider = Pos((TURNTABLE_D/4)*math.cos(math.radians(ang)), (TURNTABLE_D/4)*math.sin(math.radians(ang)), TURNTABLE_H/2) * Box(TURNTABLE_D/2 - 6, TRAY_WALL, TURNTABLE_H)
            divider = Rot(0, 0, ang) * divider
            add(divider)
        # center hub
        add(Cylinder(10, TURNTABLE_H + 4))
        # D-shaped shaft cut: round plus flat-side clipping box
        add(Cylinder(5.3 / 2, TURNTABLE_H + 8), mode=Mode.SUBTRACT)
        add(_translated(Box(6, 3, TURNTABLE_H + 10), y=-2.65 + 0.4), mode=Mode.SUBTRACT)
        # soften only vertical outer/hub edges a little
        try:
            fillet(p.edges().filter_by(Axis.Z), radius=0.8)
        except Exception:
            pass
    obj = p.part
    obj.label = "three_sector_turntable_print_part"
    obj.color = COL_TRAY
    return obj


def _scale_placeholder():
    with BuildPart() as p:
        Cylinder(SCALE_D / 2, SCALE_TH)
        # sensor bar on top
        add(_translated(_rounded_box(75, 12, 3.0, 1.0), z=SCALE_TH / 2 + 1.5))
        # small HX connector tab visual
        add(_translated(_rounded_box(20, 12, 2, 1), x=34, y=0, z=SCALE_TH / 2 + 2.5))
    obj = p.part
    obj.label = "100mm_round_scale_placeholder"
    obj.color = COL_SCALE
    return obj


def _bearing_island_and_motor_slot():
    with BuildPart() as p:
        Cylinder(ISLAND_D / 2, ISLAND_H)
        # 43x43 motor shallow location pocket with raised rim/edge blocks
        add(_translated(_rounded_box(MOTOR_SLOT + 7, MOTOR_SLOT + 7, 4.0, 3), z=ISLAND_H / 2 + 2.0))
        add(_translated(_rounded_box(MOTOR_SLOT, MOTOR_SLOT, 5.0, 2.2), z=ISLAND_H / 2 + 3.2), mode=Mode.SUBTRACT)
        # central cable/shaft relief
        add(Cylinder(8, ISLAND_H + 8), mode=Mode.SUBTRACT)
    obj = p.part
    obj.label = "isolated_load_island_with_motor_cradle"
    obj.color = Color(0.7, 0.7, 0.72, 1)
    return obj


def _motor_placeholder():
    with BuildPart() as p:
        add(_rounded_box(MOTOR_X, MOTOR_X, MOTOR_BODY_H, 4.5))
        # top metal flange
        add(_translated(_rounded_box(42.3, 42.3, 4.0, 3.0), z=MOTOR_BODY_H / 2 + 2.0))
        # front shaft boss and shaft
        add(_translated(Cylinder(11, 4.0), z=MOTOR_BODY_H / 2 + 6.0))
        add(_translated(Cylinder(MOTOR_SHAFT_D / 2, MOTOR_SHAFT_H), z=MOTOR_BODY_H / 2 + 6.0 + MOTOR_SHAFT_H / 2))
        # connector protrusion at front-left visual side
        add(_translated(_rounded_box(18, 8, 8, 1.0), x=0, y=-MOTOR_X / 2 - 3, z=-MOTOR_BODY_H / 2 + 12))
    obj = p.part
    obj.label = "nema17_stepper_placeholder"
    obj.color = COL_MOTOR
    return obj


def _qr100_placeholder():
    with BuildPart() as p:
        # front face flange, centered on local origin with depth along Y
        add(_rounded_box(QR_FACE_W, QR_FACE_TH, QR_FACE_H, 4.0))
        # body extends inward (+Y from front panel)
        add(_translated(_rounded_box(QR_BODY_W, QR_BODY_DEPTH - QR_FACE_TH, QR_BODY_H, 3.0), y=(QR_BODY_DEPTH - QR_FACE_TH)/2 + QR_FACE_TH/2))
        # scanner window inset on face
        add(_translated(_rounded_box(40, 2.0, 24, 3.0), y=-QR_FACE_TH/2 - 0.8), mode=Mode.ADD)
    obj = p.part
    obj.label = "qr100_scanner_placeholder"
    obj.color = COL_QR
    return obj


def _electronics_placeholders():
    with BuildPart() as p:
        # generic controller board
        add(_translated(_rounded_box(80, 32, 2, 1.5), y=0, z=8))
        # battery pack block
        add(_translated(_rounded_box(70, 24, 18, 4), y=0, z=-5))
        # HX711 board
        add(_translated(_rounded_box(34, 22, 2, 1.2), x=45, y=0, z=8))
    obj = p.part
    obj.label = "rear_electronics_bay_placeholders"
    obj.color = COL_BOARD
    return obj


def printable_parts():
    return {
        "base": _base_solid(),
        "lid": _lid_solid(),
        "turntable": _turntable_solid(),
    }


def printable_assembly():
    asm = AssemblyHelper("smart_pillbox_printable_assembly")
    parts = printable_parts()
    asm.add(parts["base"], "base_print_part")
    asm.add(parts["lid"], "lid_print_part")
    asm.add(_translated(parts["turntable"], z=TRAY_Z), "three_sector_turntable_print_part")
    return asm.compound(asm.children)


def full_assembly_with_placeholders():
    asm = AssemblyHelper("smart_pillbox_with_hardware_placeholders")
    parts = printable_parts()
    asm.add(parts["base"], "base_print_part")
    asm.add(parts["lid"], "lid_print_part")
    asm.add(_translated(parts["turntable"], z=TRAY_Z), "three_sector_turntable_print_part")
    asm.add(_translated(_scale_placeholder(), z=SCALE_Z), "100mm_round_scale_placeholder")
    asm.add(_translated(_bearing_island_and_motor_slot(), z=ISLAND_Z), "isolated_load_island_with_motor_cradle")
    asm.add(_translated(_motor_placeholder(), z=MOTOR_Z), "nema17_stepper_placeholder")
    # QR100 in front lower panel. Its local front face points toward -Y; body extends +Y into enclosure.
    asm.add(_translated(_qr100_placeholder(), y=FRONT_Y + QR_FACE_TH / 2, z=QR_CENTER_Z), "qr100_scanner_placeholder")
    asm.add(_translated(_electronics_placeholders(), y=E_BAY_CENTER_Y, z=E_BAY_CENTER_Z), "rear_electronics_bay_placeholders")
    return asm.compound(asm.children)


def gen_step():
    """Default CAD skill entrypoint: full assembly including hardware placeholders."""
    return full_assembly_with_placeholders()


if __name__ == "__main__":
    # Quick local smoke-test dimensions.
    shape = gen_step()
    bb = shape.bounding_box()
    print("bbox", bb.size)



