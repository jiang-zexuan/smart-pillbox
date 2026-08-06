"""Generate smart pillbox CAD artifacts with CadQuery.
Units: millimeters. Origin at enclosure footprint center; +Y back, +Z up.
"""
from __future__ import annotations

import math
from pathlib import Path
import cadquery as cq
from cadquery import exporters

OUTER_X, OUTER_Y, OUTER_Z = 165.0, 200.0, 115.0
WALL = 2.5
BASE_H, LID_H = 92.0, 24.0
CORNER_R = 10.0
FRONT_Y, BACK_Y = -OUTER_Y / 2, OUTER_Y / 2

QR_FACE_W, QR_FACE_H, QR_FACE_TH = 65.0, 53.0, 8.3
QR_BODY_W, QR_BODY_H, QR_DEPTH = 54.3, 31.8, 65.3
QR_CENTER_Z = 34.0
DISP_W, DISP_H, DISP_CENTER_Z = 70.0, 35.0, 72.0  # legacy front values; top dispensing is used now
TOP_DISPENSE_ANGLE_DEG = 132.0
TOP_DISPENSE_RADIUS = 60.0
CABLE_D, CABLE_CENTER_Z = 25.0, 35.0

SCALE_D, SCALE_TH, SCALE_Z = 100.0, 3.5, 9.0
ISLAND_D, ISLAND_H = 82.0, 7.0
ISLAND_Z = SCALE_Z + SCALE_TH / 2 + ISLAND_H / 2 + 0.8
MOTOR_X, MOTOR_H, MOTOR_SLOT = 42.3, 48.0, 43.5
MOTOR_Z = ISLAND_Z + ISLAND_H / 2 + MOTOR_H / 2
SHAFT_D, SHAFT_H = 5.0, 22.0
TRAY_D, TRAY_H, TRAY_FLOOR, TRAY_WALL = 110.0, 29.0, 4.0, 2.0
TRAY_Z = ISLAND_Z + ISLAND_H / 2 + MOTOR_H + 7.0 + TRAY_H / 2

E_BAY_W, E_BAY_D, E_BAY_H = 130.0, 42.0, 32.0
E_BAY_CENTER_Y, E_BAY_CENTER_Z = 58.0, 27.0


def wp_box(x, y, z, loc=(0,0,0), fillet=0):
    obj = cq.Workplane("XY").box(x, y, z).translate(loc)
    if fillet > 0:
        try:
            obj = obj.edges("|Z").fillet(fillet)
        except Exception:
            pass
    return obj


def cyl_z(d, h, loc=(0,0,0)):
    return cq.Workplane("XY").circle(d/2).extrude(h).translate((loc[0], loc[1], loc[2]-h/2))


def cyl_y(d, h, loc=(0,0,0)):
    return cq.Workplane("XZ").circle(d/2).extrude(h).translate((loc[0], loc[1]-h/2, loc[2]))



# ---------------- Printable detail structures built into the base ----------------
def scale_retainer_ring():
    """Physical retainer ring around the 100 mm round scale disc."""
    outer = cyl_z(108.0, 4.0, loc=(0, 0, 5.0))
    inner = cyl_z(101.0, 6.0, loc=(0, 0, 5.0))
    ring = outer.cut(inner)
    # three small anti-rotation tabs inside the recess perimeter
    for ang in [90, 210, 330]:
        x, y = 47 * math.cos(math.radians(ang)), 47 * math.sin(math.radians(ang))
        tab = wp_box(10, 4, 3, loc=(x, y, 7.0), fillet=1).rotate((0,0,0),(0,0,1),ang)
        ring = ring.union(tab)
    return ring


def load_island_print_structure():
    """Raised isolated load island that represents the force path into the scale."""
    island = cyl_z(86.0, 9.0, loc=(0, 0, 15.0))
    # visible annular relief groove around island to show it is isolated from the outer shell
    island = island.cut(cyl_z(22.0, 12.0, loc=(0, 0, 15.0)))
    # four radial ribs on island top for stiffness but still within island footprint
    for ang in [45, 135, 225, 315]:
        rib = wp_box(32, 4, 4, loc=(20, 0, 21.5), fillet=0.8).rotate((0,0,0),(0,0,1),ang)
        island = island.union(rib)
    return island


def motor_square_cradle():
    """43 mm NEMA17 simplified cradle: shallow pocket plus four corner stops."""
    z = ISLAND_Z + ISLAND_H / 2 + 4.5
    base = wp_box(54, 54, 4.0, loc=(0, 0, z), fillet=3)
    pocket = wp_box(43.5, 43.5, 5.0, loc=(0, 0, z + 1.0), fillet=2)
    cradle = base.cut(pocket)
    for sx in [-1, 1]:
        for sy in [-1, 1]:
            cradle = cradle.union(wp_box(7, 7, 8, loc=(sx*24, sy*24, z+3), fillet=1))
    # central shaft/cable relief
    cradle = cradle.cut(cyl_z(14, 12, loc=(0,0,z+2)))
    return cradle


def rear_electronics_tray():
    """Rear tray for main board/battery/HX711, with raised lips."""
    tray_z = 7.5
    tray = wp_box(128, 40, 3.0, loc=(0, E_BAY_CENTER_Y, tray_z), fillet=3)
    # side and back lips
    tray = tray.union(wp_box(128, 3, 10, loc=(0, E_BAY_CENTER_Y + 20, tray_z + 5), fillet=1))
    tray = tray.union(wp_box(3, 40, 10, loc=(-64, E_BAY_CENTER_Y, tray_z + 5), fillet=1))
    tray = tray.union(wp_box(3, 40, 10, loc=(64, E_BAY_CENTER_Y, tray_z + 5), fillet=1))
    # two low board rails
    tray = tray.union(wp_box(105, 2, 4, loc=(0, E_BAY_CENTER_Y - 10, tray_z + 3.5), fillet=0.5))
    tray = tray.union(wp_box(105, 2, 4, loc=(0, E_BAY_CENTER_Y + 8, tray_z + 3.5), fillet=0.5))
    return tray


def internal_cable_channel():
    """Raised-edge cable channel from QR/front and scale center toward rear electronics bay."""
    z = 11.0
    channel = wp_box(18, 98, 2.0, loc=(0, 6, z), fillet=1)
    # hollow the visual center so it reads like a channel with side walls
    channel = channel.cut(wp_box(12, 94, 3.0, loc=(0, 6, z + 0.2), fillet=0.8))
    # branch to QR100 lower front opening
    branch = wp_box(14, 44, 2.0, loc=(0, -67, z), fillet=1).cut(wp_box(8, 40, 3.0, loc=(0, -67, z + 0.2), fillet=0.8))
    return channel.union(branch)


def qr100_stepped_frame():
    """Thickened printable stepped frame around QR100 so it is not just a flat hole."""
    outer = wp_box(QR_FACE_W + 12, 8.0, QR_FACE_H + 12, loc=(0, FRONT_Y - 1.8, QR_CENTER_Z), fillet=4)
    inner = wp_box(QR_FACE_W + 0.8, 10.0, QR_FACE_H + 0.8, loc=(0, FRONT_Y - 1.8, QR_CENTER_Z), fillet=2.5)
    frame = outer.cut(inner)
    # two small side latch nubs, representing removable QR module retention
    frame = frame.union(wp_box(4, 5, 9, loc=(-(QR_FACE_W/2+4), FRONT_Y-3.5, QR_CENTER_Z), fillet=1))
    frame = frame.union(wp_box(4, 5, 9, loc=((QR_FACE_W/2+4), FRONT_Y-3.5, QR_CENTER_Z), fillet=1))
    return frame



def base_part():
    base = wp_box(OUTER_X, OUTER_Y, BASE_H, loc=(0,0,BASE_H/2), fillet=CORNER_R)
    # open top cavity
    cavity = wp_box(OUTER_X-2*WALL, OUTER_Y-2*WALL, BASE_H-WALL+3, loc=(0,0,WALL+(BASE_H-WALL+3)/2), fillet=max(CORNER_R-WALL,1))
    base = base.cut(cavity)
    # no front dispense opening: medicine is taken through the top sector opening
    # QR100 stepped recess: outer face shallow, inner body deep clearance
    base = base.cut(wp_box(QR_FACE_W+0.7, WALL+2, QR_FACE_H+0.7, loc=(0, FRONT_Y-0.2, QR_CENTER_Z), fillet=2.5))
    base = base.cut(wp_box(QR_BODY_W+1.0, QR_DEPTH+8, QR_BODY_H+1.0, loc=(0, FRONT_Y+QR_DEPTH/2, QR_CENTER_Z), fillet=2.0))
    # front functional bezels and physical mounting details
    base = base.union(qr100_stepped_frame())
    # no extra shelf under dispense window: front panel should only show dispense opening + QR100 opening
    # back cable hole
    base = base.cut(cyl_y(CABLE_D, WALL+10, loc=(0, BACK_Y+1, CABLE_CENTER_Z)))
    # rear electronics bay pocket/visible bay
    base = base.cut(wp_box(E_BAY_W+2, E_BAY_D+2, E_BAY_H, loc=(0, E_BAY_CENTER_Y, E_BAY_CENTER_Z), fillet=3))
    # scale recess visible in bottom
    base = base.cut(cyl_z(SCALE_D+4, SCALE_TH+1.0, loc=(0,0,SCALE_Z)))
    # physical internal mounting structures
    base = base.union(scale_retainer_ring())
    base = base.union(load_island_print_structure())
    base = base.union(motor_square_cradle())
    base = base.union(rear_electronics_tray())
    base = base.union(internal_cable_channel())
    return base



def top_sector_opening_cut():
    """Cutter for top dispensing sector: slightly larger than one 120 degree tray sector."""
    angle = math.radians(TOP_DISPENSE_ANGLE_DEG)
    r = TOP_DISPENSE_RADIUS
    a1 = -math.pi / 2 - angle / 2
    a2 = -math.pi / 2 + angle / 2
    pts = [(0, 0)]
    steps = 20
    for i in range(steps + 1):
        a = a1 + (a2 - a1) * i / steps
        pts.append((r * math.cos(a), r * math.sin(a)))
    pts.append((0, 0))
    return cq.Workplane("XY").polyline(pts).close().extrude(34).translate((0, 0, BASE_H + LID_H - 20))
def lid_part():
    zc = BASE_H + LID_H/2 - 1
    lid = wp_box(OUTER_X+0.4, OUTER_Y+0.4, LID_H, loc=(0,0,zc), fillet=CORNER_R)
    inner_h = LID_H - 3.0
    lid = lid.cut(wp_box(OUTER_X-2*WALL+0.8, OUTER_Y-2*WALL+0.8, inner_h+1, loc=(0,0,BASE_H-1+inner_h/2), fillet=CORNER_R-WALL))
    lid = lid.cut(wp_box(OUTER_X-4, OUTER_Y-4, 4, loc=(0,0,BASE_H-3), fillet=4))
    # perforation array over tray region
    for r in [0,13,26,39,50]:
        count = 1 if r == 0 else max(6, int(2*math.pi*r/16))
        for i in range(count):
            a = 0 if count == 1 else 2*math.pi*i/count
            x, y = r*math.cos(a), r*math.sin(a)
            if x*x+y*y <= 52*52:
                lid = lid.cut(cyl_z(5.0, 10, loc=(x,y,BASE_H+LID_H-3)))
    # top sector dispensing opening, slightly larger than one 120-degree tray sector
    lid = lid.cut(top_sector_opening_cut())
    return lid


def d_shaft_cut(height):
    round_cut = cyl_z(5.3, height, loc=(0,0,0))
    flat = wp_box(8, 4, height+2, loc=(0, -2.65+0.4, 0))
    return round_cut.union(flat)


def turntable_part():
    # base disk + rim + dividers + hub
    tray = cyl_z(TRAY_D, TRAY_FLOOR, loc=(0,0,TRAY_FLOOR/2))
    rim = cyl_z(TRAY_D, TRAY_H, loc=(0,0,TRAY_H/2)).cut(cyl_z(TRAY_D-2*TRAY_WALL, TRAY_H+2, loc=(0,0,TRAY_H/2)))
    tray = tray.union(rim)
    for ang in [0,120,240]:
        divider = wp_box(TRAY_D/2-6, TRAY_WALL, TRAY_H, loc=(TRAY_D/4,0,TRAY_H/2), fillet=0.5).rotate((0,0,0),(0,0,1),ang)
        tray = tray.union(divider)
    tray = tray.union(cyl_z(20, TRAY_H+4, loc=(0,0,(TRAY_H+4)/2)))
    tray = tray.cut(d_shaft_cut(TRAY_H+10).translate((0,0,(TRAY_H+4)/2)))
    try:
        tray = tray.edges("|Z").fillet(0.6)
    except Exception:
        pass
    return tray


def scale_placeholder():
    s = cyl_z(SCALE_D, SCALE_TH, loc=(0,0,SCALE_Z))
    s = s.union(wp_box(75, 12, 3, loc=(0,0,SCALE_Z+SCALE_TH/2+1.5), fillet=1))
    s = s.union(wp_box(20, 12, 2, loc=(34,0,SCALE_Z+SCALE_TH/2+2.5), fillet=1))
    return s


def load_island():
    island = cyl_z(ISLAND_D, ISLAND_H, loc=(0,0,ISLAND_Z))
    rim = wp_box(MOTOR_SLOT+7, MOTOR_SLOT+7, 4, loc=(0,0,ISLAND_Z+ISLAND_H/2+2), fillet=3)
    pocket = wp_box(MOTOR_SLOT, MOTOR_SLOT, 5, loc=(0,0,ISLAND_Z+ISLAND_H/2+3), fillet=2)
    island = island.union(rim).cut(pocket).cut(cyl_z(16, ISLAND_H+10, loc=(0,0,ISLAND_Z)))
    return island


def motor_placeholder():
    m = wp_box(MOTOR_X, MOTOR_X, MOTOR_H, loc=(0,0,MOTOR_Z), fillet=4)
    m = m.union(wp_box(42.3,42.3,4,loc=(0,0,MOTOR_Z+MOTOR_H/2+2),fillet=3))
    m = m.union(cyl_z(22,4,loc=(0,0,MOTOR_Z+MOTOR_H/2+6)))
    m = m.union(cyl_z(SHAFT_D,SHAFT_H,loc=(0,0,MOTOR_Z+MOTOR_H/2+6+SHAFT_H/2)))
    m = m.union(wp_box(18,8,8,loc=(0,-MOTOR_X/2-3,MOTOR_Z-MOTOR_H/2+12),fillet=1))
    return m


def qr100_placeholder():
    q = wp_box(QR_FACE_W, QR_FACE_TH, QR_FACE_H, loc=(0, FRONT_Y+QR_FACE_TH/2, QR_CENTER_Z), fillet=4)
    q = q.union(wp_box(QR_BODY_W, QR_DEPTH-QR_FACE_TH, QR_BODY_H, loc=(0, FRONT_Y+QR_FACE_TH+(QR_DEPTH-QR_FACE_TH)/2, QR_CENTER_Z), fillet=3))
    q = q.union(wp_box(40, 1.5, 24, loc=(0, FRONT_Y-1, QR_CENTER_Z), fillet=2))
    return q


def electronics_placeholder():
    e = wp_box(80,32,2,loc=(0,E_BAY_CENTER_Y,E_BAY_CENTER_Z+8),fillet=1)
    e = e.union(wp_box(70,24,18,loc=(0,E_BAY_CENTER_Y,E_BAY_CENTER_Z-5),fillet=4))
    e = e.union(wp_box(34,22,2,loc=(45,E_BAY_CENTER_Y,E_BAY_CENTER_Z+8),fillet=1))
    return e


def shifted_turntable():
    return turntable_part().translate((0,0,TRAY_Z-TRAY_H/2))


def printable_assembly():
    return cq.Assembly(name="smart_pillbox_printable").add(base_part(), name="base").add(lid_part(), name="lid").add(shifted_turntable(), name="turntable")


def full_assembly():
    asm = printable_assembly()
    asm.add(scale_placeholder(), name="100mm_round_scale")
    asm.add(load_island(), name="isolated_load_island")
    asm.add(motor_placeholder(), name="nema17_stepper")
    asm.add(qr100_placeholder(), name="qr100_scanner")
    asm.add(electronics_placeholder(), name="rear_electronics_placeholders")
    return asm


def export_all(outdir: Path):
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir/"stl").mkdir(exist_ok=True)
    full = full_assembly()
    printable = printable_assembly()
    full.save(str(outdir/"smart_pillbox_with_placeholders.step"), exportType="STEP")
    printable.save(str(outdir/"smart_pillbox_printable.step"), exportType="STEP")
    parts = {"base": base_part(), "lid": lid_part(), "turntable": turntable_part()}
    for name, obj in parts.items():
        exporters.export(obj, str(outdir/f"smart_pillbox_{name}.step"), exportType="STEP")
        exporters.export(obj, str(outdir/"stl"/f"smart_pillbox_{name}.stl"), exportType="STL", tolerance=0.15, angularTolerance=0.15)
    return outdir

if __name__ == "__main__":
    out = export_all(Path(__file__).resolve().parent/"output")
    print(out)







