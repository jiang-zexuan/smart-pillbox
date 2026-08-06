"""Minimal smart pillbox base V1.

Units: millimeters.
Coordinate system: origin at footprint center, +Y toward rear, +Z up.

This version intentionally contains only:
- rounded open-top base shell
- one front QR100 insertion opening

It intentionally excludes the previous internal scale/motor/electronics/cable
features so parts can be added back one by one.
"""
from __future__ import annotations

from pathlib import Path
import math

import cadquery as cq
from cadquery import exporters


# Overall base footprint retained from the earlier approved printable footprint.
OUTER_X = 165.0
OUTER_Y = 200.0
# Base height is finalized after the motor/tray stack constants below so the
# shell rim can be flush with the medicine tray top.

WALL = 2.5
CORNER_R = 10.0
FRONT_Y = -OUTER_Y / 2.0

# QR100 front opening requested for the current simple-fit iteration.
QR_OPENING_W = 55.0
QR_OPENING_H = 44.0
QR_CENTER_Z = 39.0
QR_CORNER_R = 2.0

# ZY-QR100 scanner placeholder, dimensions read from the supplied image PDF.
# Front view: 65 x 53 mm. Side/top views: front lip 8.3 mm, rear body about
# 23.3 mm deep, body width about 54.3 mm, total side envelope about 31.8 mm.
QR100_FRONT_W = 65.0
QR100_FRONT_H = 53.0
QR100_FRONT_T = 8.3
QR100_BODY_W = 54.3
QR100_BODY_H = 31.8
QR100_BODY_D = 23.3
QR100_TAB_SPACING_X = 43.0
QR100_TAB_W = 7.0
QR100_TAB_H = 5.0
QR100_TAB_HOLE_D = 2.3
QR100_SCAN_WINDOW_W = 38.0
QR100_SCAN_WINDOW_H = 22.0
QR100_LENS_D = 7.0

# Rear branding near the upper back wall. Recessed/debossed so it prints
# without unsupported raised fine features.
REAR_LOGO_TEXT = "Design by JZX"
REAR_LOGO_SIZE = 10.5
REAR_LOGO_DEPTH = 1.2
REAR_LOGO_CENTER_Z = 101.0

# Rear cable pass-through.
CABLE_HOLE_D = 30.0
CABLE_CENTER_Z = 35.0

# Simplified round HX711/load-cell disc placement.
SENSOR_D = 100.0
SENSOR_H = 30.0
SENSOR_CLEARANCE = 2.0
SENSOR_RETAINER_H = 15.0
SENSOR_RETAINER_WALL = 5.0
SENSOR_RETAINER_INNER_D = SENSOR_D + SENSOR_CLEARANCE
SENSOR_RETAINER_OUTER_D = SENSOR_RETAINER_INNER_D + 2 * SENSOR_RETAINER_WALL
# Keep the front edge of the retaining ring at least 30 mm behind the QR100/front opening.
SENSOR_FRONT_GAP = 30.0
SENSOR_CENTER_Y = FRONT_Y + SENSOR_FRONT_GAP + SENSOR_RETAINER_OUTER_D / 2.0

# Rear hardware / wiring compartment divider.
REAR_DIVIDER_T = 3.0
REAR_DIVIDER_H_RATIO = 0.5
REAR_DIVIDER_NOTCH_W = 18.0
REAR_DIVIDER_NOTCH_H = 56.0 / 3.0
REAR_DIVIDER_CLEARANCE_FROM_RING = 2.0
REAR_DIVIDER_Y = (
    SENSOR_CENTER_Y
    + SENSOR_RETAINER_OUTER_D / 2.0
    + REAR_DIVIDER_CLEARANCE_FROM_RING
    + REAR_DIVIDER_T / 2.0
)

# Separate motor support on top of the simplified HX711/load-cell cylinder.
MOTOR_MOUNT_INNER_D = 104.0
MOTOR_MOUNT_OUTER_D = 114.0
MOTOR_MOUNT_TOP_TH = 3.0
# Inverted-cup motor mount: its inner ceiling sits directly on the HX711 top face,
# and the skirt only sleeves downward 7 mm so it does not fight the base retainer ring.
MOTOR_MOUNT_OVERLAP_H = 7.0
MOTOR_MOUNT_H = MOTOR_MOUNT_TOP_TH + MOTOR_MOUNT_OVERLAP_H
SENSOR_TOP_Z = WALL + SENSOR_H
MOTOR_MOUNT_BOTTOM_Z = SENSOR_TOP_Z - MOTOR_MOUNT_OVERLAP_H
MOTOR_MOUNT_TOP_Z = SENSOR_TOP_Z + MOTOR_MOUNT_TOP_TH
MOTOR_HOLE_SPACING = 31.0
MOTOR_FIX_BOSS_H = 0.0
MOTOR_CENTER_RELIEF_D = 18.0
MOTOR_CORNER_CLEARANCE = 1.2
MOTOR_CORNER_FENCE_T = 4.0
MOTOR_CORNER_FENCE_LEN = 14.0
MOTOR_CORNER_FENCE_H = 8.0

# Simplified NEMA17-like stepper motor placeholder.
MOTOR_BODY_XY = 42.3
MOTOR_BODY_H = 48.0
MOTOR_FLANGE_TH = 4.0
MOTOR_SHAFT_D = 5.0
MOTOR_SHAFT_FLAT_TO_ROUND = 4.5
MOTOR_SHAFT_H = 22.0
MOTOR_CONNECTOR_W = 16.0
MOTOR_CONNECTOR_D = 7.0
MOTOR_CONNECTOR_H = 8.0

# Six-compartment medicine turntable driven by the D-shaped motor shaft.
MOTOR_FACE_TOP_Z = MOTOR_MOUNT_TOP_Z + MOTOR_BODY_H + MOTOR_FLANGE_TH
MOTOR_BOSS_H = 4.0
MOTOR_BOSS_TOP_Z = MOTOR_FACE_TOP_Z + MOTOR_BOSS_H
TRAY_D = 112.0
TRAY_H = 25.0
TRAY_LID_CLEARANCE = 3.0
TRAY_FLOOR_H = 4.0
TRAY_RIM_W = 2.2
TRAY_DIVIDER_W = 2.0
TRAY_HUB_D = 20.0
TRAY_D_HOLE_D = 5.6
TRAY_D_HOLE_FLAT_TO_ROUND = 5.0
TRAY_BOTTOM_Z = MOTOR_BOSS_TOP_Z
TRAY_TOP_Z = TRAY_BOTTOM_Z + TRAY_H
# Keep the pillbox wall/lid height at the original envelope, but leave a
# motion clearance above the lowered medicine tray.
BASE_H = TRAY_TOP_Z + TRAY_LID_CLEARANCE
REAR_DIVIDER_H = BASE_H * REAR_DIVIDER_H_RATIO
REAR_DIVIDER_HOLE_CENTER_Z = WALL + REAR_DIVIDER_H / 2.0

# TJC8048X270_011R 7-inch serial HMI screen, from the supplied PDF.
# Coordinate on right side: screen length along Y, height along Z, thickness along X.
SCREEN_PCB_L = 181.0
SCREEN_PCB_H = 108.0
SCREEN_TOTAL_T = 8.8
SCREEN_LCD_FRAME_L = 164.9
SCREEN_LCD_FRAME_H = 100.0
SCREEN_LCD_AA_L = 154.08
SCREEN_LCD_AA_H = 85.92
SCREEN_CENTER_Y = 0.0
SCREEN_CENTER_Z = BASE_H / 2.0
SCREEN_OUTER_FACE_X = OUTER_X / 2.0
# Drawing page 7: 4-∅3.20 mounting holes. Back view dimensions show
# center-to-center pitch approximately 174.60 x 101.60 mm.
SCREEN_MOUNT_HOLE_D = 3.2
SCREEN_MOUNT_HOLE_PITCH_Y = 174.60
SCREEN_MOUNT_HOLE_PITCH_Z = 101.60
SCREEN_MOUNT_HOLE_YS = (-SCREEN_MOUNT_HOLE_PITCH_Y / 2.0, SCREEN_MOUNT_HOLE_PITCH_Y / 2.0)
SCREEN_MOUNT_HOLE_ZS = (
    SCREEN_CENTER_Z - SCREEN_MOUNT_HOLE_PITCH_Z / 2.0,
    SCREEN_CENTER_Z + SCREEN_MOUNT_HOLE_PITCH_Z / 2.0,
)

# Symmetric side ventilation/lightening holes requested after the screen mount update:
# 6 evenly distributed circular holes on each long side, arranged as 3 columns x 2 rows.
SIDE_ROUND_HOLE_D = 20.0
SIDE_ROUND_HOLE_YS = (-55.0, 0.0, 55.0)
SIDE_ROUND_HOLE_ZS = (35.0, 85.0)

# Separate printable top lid.
LID_PLATE_TH = 3.0
LID_SKIRT_H = 7.0
LID_RAISE_H = 10.0
LID_INTERNAL_OVERLAP = 0.2
LID_SKIRT_CONNECTOR_TH = 0.6
LID_SKIRT_WALL = 2.0
LID_FIT_CLEARANCE = 0.6
LID_TOTAL_H = LID_PLATE_TH + LID_SKIRT_H + LID_RAISE_H
LID_TOP_Z = BASE_H + LID_RAISE_H + LID_PLATE_TH
LID_OBSERVE_HOLE_D = 6.0
LID_OBSERVE_HOLE_XS = (-60.0, -40.0, -20.0, 0.0, 20.0, 40.0, 60.0)
LID_OBSERVE_HOLE_YS = (-70.0, -35.0, 0.0, 35.0, 70.0)
LID_PICKOUT_R = 59.0
LID_PICKOUT_ANGLE_DEG = 70.0
LID_PICKOUT_CENTER_ANGLE_DEG = -90.0
LID_PICKOUT_CORNER_R = 6.0
LID_CHAMFER = 0.7


def rounded_box(x: float, y: float, z: float, loc=(0, 0, 0), radius: float = 0) -> cq.Workplane:
    """Create a box with optional vertical-edge fillets."""
    obj = cq.Workplane("XY").box(x, y, z).translate(loc)
    if radius > 0:
        obj = obj.edges("|Z").fillet(radius)
    return obj


def cylinder_y(diameter: float, length: float, loc=(0, 0, 0)) -> cq.Workplane:
    """Create a cylinder running along Y, centered at loc."""
    return (
        cq.Workplane("XZ")
        .circle(diameter / 2.0)
        .extrude(length)
        .translate((loc[0], loc[1] + length / 2.0, loc[2]))
    )


def cylinder_x(diameter: float, length: float, loc=(0, 0, 0)) -> cq.Workplane:
    """Create a cylinder running along X, centered at loc."""
    return (
        cq.Workplane("YZ")
        .circle(diameter / 2.0)
        .extrude(length)
        .translate((loc[0] - length / 2.0, loc[1], loc[2]))
    )


def cylinder_z(diameter: float, height: float, loc=(0, 0, 0)) -> cq.Workplane:
    """Create a vertical cylinder centered at loc."""
    return (
        cq.Workplane("XY")
        .circle(diameter / 2.0)
        .extrude(height)
        .translate((loc[0], loc[1], loc[2] - height / 2.0))
    )


def sector_prism_z(
    radius: float,
    angle_deg: float,
    height: float,
    loc=(0, 0, 0),
    center_angle_deg: float = -90.0,
    segments: int = 32,
) -> cq.Workplane:
    """Vertical sector prism used for the lid pick-out opening."""
    start = math.radians(center_angle_deg - angle_deg / 2.0)
    end = math.radians(center_angle_deg + angle_deg / 2.0)
    pts = [(loc[0], loc[1])]
    for i in range(segments + 1):
        a = start + (end - start) * i / segments
        pts.append((loc[0] + radius * math.cos(a), loc[1] + radius * math.sin(a)))
    pts.append((loc[0], loc[1]))
    return (
        cq.Workplane("XY")
        .polyline(pts)
        .close()
        .extrude(height)
        .translate((0, 0, loc[2] - height / 2.0))
    )


def rounded_sector_prism_z(
    radius: float,
    angle_deg: float,
    corner_radius: float,
    height: float,
    loc=(0, 0, 0),
    center_angle_deg: float = -90.0,
    segments: int = 32,
) -> cq.Workplane:
    """Sector prism with rounded apex and outer radial corners for lid cutouts."""
    def unit(angle_rad: float) -> tuple[float, float]:
        return (math.cos(angle_rad), math.sin(angle_rad))

    def rot90(v: tuple[float, float], sign: int) -> tuple[float, float]:
        x, y = v
        return (-sign * y, sign * x)

    def add(p: tuple[float, float], q: tuple[float, float]) -> tuple[float, float]:
        return (p[0] + q[0], p[1] + q[1])

    def mul(v: tuple[float, float], s: float) -> tuple[float, float]:
        return (v[0] * s, v[1] * s)

    def arc_points(
        center: tuple[float, float],
        r: float,
        p0: tuple[float, float],
        p1: tuple[float, float],
        steps: int = 8,
        shortest: bool = True,
    ) -> list[tuple[float, float]]:
        a0 = math.atan2(p0[1] - center[1], p0[0] - center[0])
        a1 = math.atan2(p1[1] - center[1], p1[0] - center[0])
        if shortest:
            delta = (a1 - a0 + math.pi) % (2.0 * math.pi) - math.pi
        else:
            delta = a1 - a0
            if delta < 0:
                delta += 2.0 * math.pi
        return [
            (center[0] + r * math.cos(a0 + delta * i / steps), center[1] + r * math.sin(a0 + delta * i / steps))
            for i in range(1, steps + 1)
        ]

    origin = (loc[0], loc[1])
    start = math.radians(center_angle_deg - angle_deg / 2.0)
    end = math.radians(center_angle_deg + angle_deg / 2.0)
    u_start = unit(start)
    u_end = unit(end)
    theta = math.radians(angle_deg)
    cr = min(corner_radius, radius * 0.18)

    # Apex fillet: trim both radial edges and connect with an inner tangent arc.
    apex_trim = cr / math.tan(theta / 2.0)
    apex_center_dist = cr / math.sin(theta / 2.0)
    u_mid = unit(math.radians(center_angle_deg))
    apex_center = add(origin, mul(u_mid, apex_center_dist))
    apex_start_tangent = add(origin, mul(u_start, apex_trim))
    apex_end_tangent = add(origin, mul(u_end, apex_trim))

    # Outer corner fillets: radial edge is perpendicular to the outer circular edge.
    outer_center_dist = math.sqrt(max((radius - cr) ** 2 - cr**2, 0.0))
    n_start = rot90(u_start, 1)
    n_end = rot90(u_end, -1)
    start_fillet_center = add(add(origin, mul(u_start, outer_center_dist)), mul(n_start, cr))
    end_fillet_center = add(add(origin, mul(u_end, outer_center_dist)), mul(n_end, cr))
    start_line_tangent = add(origin, mul(u_start, outer_center_dist))
    end_line_tangent = add(origin, mul(u_end, outer_center_dist))
    start_outer_tangent = add(origin, mul((start_fillet_center[0] - origin[0], start_fillet_center[1] - origin[1]), radius / (radius - cr)))
    end_outer_tangent = add(origin, mul((end_fillet_center[0] - origin[0], end_fillet_center[1] - origin[1]), radius / (radius - cr)))

    pts = [apex_start_tangent, start_line_tangent]
    pts.extend(arc_points(start_fillet_center, cr, start_line_tangent, start_outer_tangent, steps=8))
    pts.extend(arc_points(origin, radius, start_outer_tangent, end_outer_tangent, steps=max(segments, 16), shortest=False))
    pts.extend(arc_points(end_fillet_center, cr, end_outer_tangent, end_line_tangent, steps=8))
    pts.append(apex_end_tangent)
    pts.extend(arc_points(apex_center, cr, apex_end_tangent, apex_start_tangent, steps=12))

    return (
        cq.Workplane("XY")
        .polyline(pts)
        .close()
        .offset2D(corner_radius, kind="arc")
        .offset2D(-corner_radius, kind="arc")
        .extrude(height)
        .translate((0, 0, loc[2] - height / 2.0))
    )


def d_profile_z(diameter: float, flat_to_round: float, height: float, loc=(0, 0, 0)) -> cq.Workplane:
    """Vertical D-shaped profile, flat side facing -Y."""
    shaft = cylinder_z(diameter, height, loc=loc)
    flat_plane_y = loc[1] + diameter / 2.0 - flat_to_round
    flat_cut_depth = diameter
    flat_cut = rounded_box(
        diameter + 2.0,
        flat_cut_depth,
        height + 2.0,
        loc=(loc[0], flat_plane_y - flat_cut_depth / 2.0, loc[2]),
        radius=0,
    )
    return shaft.cut(flat_cut)


def d_shaft_z(diameter: float, height: float, loc=(0, 0, 0)) -> cq.Workplane:
    """Vertical D-shaped shaft: 5 mm round with a flat side, flat facing -Y."""
    return d_profile_z(diameter, MOTOR_SHAFT_FLAT_TO_ROUND, height, loc)


def rear_logo_engraving_cut() -> cq.Workplane:
    """Back-face recessed text cut, centered near the upper rear wall."""
    # XZ text extrudes along -Y. Placing its front at OUTER_Y/2 cuts from the
    # exterior rear face inward by REAR_LOGO_DEPTH.
    return (
        cq.Workplane("XZ")
        .text(
            REAR_LOGO_TEXT,
            REAR_LOGO_SIZE,
            REAR_LOGO_DEPTH,
            halign="center",
            valign="center",
            font="Arial",
        )
        # Mirror the cut geometry horizontally so the recessed text reads
        # correctly when viewed from the outside rear face.
        .mirror("YZ")
        .translate((0, OUTER_Y / 2.0, REAR_LOGO_CENTER_Z))
    )


def qr100_scanner_placeholder() -> cq.Workplane:
    """Separate simplified ZY-QR100 scanner model inserted into the front opening."""
    # Coordinate mapping:
    # X = scanner width, Y = depth through the front wall, Z = scanner height.
    # The front lip sits outside the pillbox front; the rear body enters the QR opening.
    front_lip_y = FRONT_Y - QR100_FRONT_T / 2.0
    front_face_y = FRONT_Y - QR100_FRONT_T
    zc = QR_CENTER_Z

    scanner = rounded_box(
        QR100_FRONT_W,
        QR100_FRONT_T,
        QR100_FRONT_H,
        loc=(0, front_lip_y, zc),
        radius=3.0,
    )

    # Rear body, sized to pass through the 55 x 44 mm base opening.
    scanner = scanner.union(
        rounded_box(
            QR100_BODY_W,
            QR100_BODY_D,
            QR100_BODY_H,
            loc=(0, FRONT_Y + QR100_BODY_D / 2.0, zc),
            radius=3.0,
        )
    )

    # Four small mounting ears visible in the manual front view.
    for sx in (-1, 1):
        for sz in (-1, 1):
            tab = rounded_box(
                QR100_TAB_W,
                QR100_FRONT_T,
                QR100_TAB_H,
                loc=(
                    sx * QR100_TAB_SPACING_X / 2.0,
                    front_lip_y,
                    zc + sz * (QR100_FRONT_H / 2.0 + QR100_TAB_H / 2.0 - 0.8),
                ),
                radius=1.0,
            )
            tab = tab.cut(
                cylinder_y(
                    QR100_TAB_HOLE_D,
                    QR100_FRONT_T + 2.0,
                    loc=(
                        sx * QR100_TAB_SPACING_X / 2.0,
                        front_lip_y - QR100_FRONT_T / 2.0 - 1.0,
                        zc + sz * (QR100_FRONT_H / 2.0 + QR100_TAB_H / 2.0 - 0.8),
                    ),
                )
            )
            scanner = scanner.union(tab)

    # Front scan window recess and central lens/bullseye details.
    window_recess = rounded_box(
        QR100_SCAN_WINDOW_W,
        1.4,
        QR100_SCAN_WINDOW_H,
        loc=(0, front_face_y + 0.6, zc),
        radius=0.5,
    )
    scanner = scanner.cut(window_recess)
    scanner = scanner.cut(cylinder_y(QR100_LENS_D, 1.5, loc=(0, front_face_y - 0.2, zc)))
    scanner = scanner.cut(cylinder_y(QR100_LENS_D * 0.42, 1.7, loc=(0, front_face_y - 0.3, zc)))

    # Small rear connector features for orientation/readability.
    scanner = scanner.union(
        rounded_box(
            7.0,
            1.0,
            12.0,
            loc=(-11.0, FRONT_Y + QR100_BODY_D + 0.5, zc - 1.0),
            radius=0.3,
        )
    )
    scanner = scanner.union(
        rounded_box(
            14.0,
            1.0,
            6.0,
            loc=(11.0, FRONT_Y + QR100_BODY_D + 0.5, zc - 1.0),
            radius=0.3,
        )
    )
    return scanner


def sensor_retainer_ring() -> cq.Workplane:
    """Integral 15 mm high round retainer frame for the separate sensor cylinder."""
    outer = cylinder_z(
        SENSOR_RETAINER_OUTER_D,
        SENSOR_RETAINER_H,
        loc=(0, SENSOR_CENTER_Y, WALL + SENSOR_RETAINER_H / 2.0),
    )
    inner = cylinder_z(
        SENSOR_RETAINER_INNER_D,
        SENSOR_RETAINER_H + 2.0,
        loc=(0, SENSOR_CENTER_Y, WALL + SENSOR_RETAINER_H / 2.0),
    )
    return outer.cut(inner)


def hx711_sensor_placeholder() -> cq.Workplane:
    """Separate simplified HX711/load-cell + 100 mm round disc placeholder."""
    return cylinder_z(
        SENSOR_D,
        SENSOR_H,
        loc=(0, SENSOR_CENTER_Y, WALL + SENSOR_H / 2.0),
    )


def motor_mount_placeholder() -> cq.Workplane:
    """Separate downward-open shallow cylindrical cup for holding the motor."""
    cup_total_h = MOTOR_MOUNT_TOP_Z - MOTOR_MOUNT_BOTTOM_Z
    zc = MOTOR_MOUNT_BOTTOM_Z + cup_total_h / 2.0
    outer = cylinder_z(MOTOR_MOUNT_OUTER_D, cup_total_h, loc=(0, SENSOR_CENTER_Y, zc))
    # Open the cup from below only up to the HX711 top plane. The remaining
    # 3 mm ceiling is the actual load-transfer face sitting on the sensor top.
    inner_void = cylinder_z(
        MOTOR_MOUNT_INNER_D,
        MOTOR_MOUNT_OVERLAP_H + 1.0,
        loc=(0, SENSOR_CENTER_Y, (MOTOR_MOUNT_BOTTOM_Z - 1.0 + SENSOR_TOP_Z) / 2.0),
    )
    mount = outer.cut(inner_void)
    mount = mount.cut(
        cylinder_z(
            MOTOR_CENTER_RELIEF_D,
            MOTOR_MOUNT_TOP_TH + 4.0,
            loc=(0, SENSOR_CENTER_Y, MOTOR_MOUNT_TOP_Z - MOTOR_MOUNT_TOP_TH / 2.0),
        )
    )

    # Four L-shaped corner fences that cradle the NEMA17 square motor corners.
    for sx in (-1, 1):
        for sy in (-1, 1):
            corner_x = sx * (MOTOR_BODY_XY / 2.0 + MOTOR_CORNER_CLEARANCE / 2.0)
            corner_y = SENSOR_CENTER_Y + sy * (MOTOR_BODY_XY / 2.0 + MOTOR_CORNER_CLEARANCE / 2.0)
            fence_z = MOTOR_MOUNT_TOP_Z + MOTOR_CORNER_FENCE_H / 2.0
            x_leg = rounded_box(
                MOTOR_CORNER_FENCE_T,
                MOTOR_CORNER_FENCE_LEN,
                MOTOR_CORNER_FENCE_H,
                loc=(corner_x + sx * MOTOR_CORNER_FENCE_T / 2.0, corner_y, fence_z),
                radius=0.7,
            )
            y_leg = rounded_box(
                MOTOR_CORNER_FENCE_LEN,
                MOTOR_CORNER_FENCE_T,
                MOTOR_CORNER_FENCE_H,
                loc=(corner_x, corner_y + sy * MOTOR_CORNER_FENCE_T / 2.0, fence_z),
                radius=0.7,
            )
            mount = mount.union(x_leg).union(y_leg)
    return mount


def stepper_motor_placeholder() -> cq.Workplane:
    """Separate simplified NEMA17-like stepper motor model with shaft."""
    motor_bottom_z = MOTOR_MOUNT_TOP_Z
    body_zc = motor_bottom_z + MOTOR_BODY_H / 2.0
    motor = rounded_box(
        MOTOR_BODY_XY,
        MOTOR_BODY_XY,
        MOTOR_BODY_H,
        loc=(0, SENSOR_CENTER_Y, body_zc),
        radius=3.0,
    )
    # Front/top flange and central boss.
    motor = motor.union(
        rounded_box(
            MOTOR_BODY_XY,
            MOTOR_BODY_XY,
            MOTOR_FLANGE_TH,
            loc=(0, SENSOR_CENTER_Y, motor_bottom_z + MOTOR_BODY_H + MOTOR_FLANGE_TH / 2.0),
            radius=2.5,
        )
    )
    top_z = motor_bottom_z + MOTOR_BODY_H + MOTOR_FLANGE_TH
    motor = motor.union(cylinder_z(22.0, MOTOR_BOSS_H, loc=(0, SENSOR_CENTER_Y, top_z + MOTOR_BOSS_H / 2.0)))
    motor = motor.union(d_shaft_z(MOTOR_SHAFT_D, MOTOR_SHAFT_H, loc=(0, SENSOR_CENTER_Y, top_z + 4.0 + MOTOR_SHAFT_H / 2.0)))

    # Small connector block on one side.
    motor = motor.union(
        rounded_box(
            MOTOR_CONNECTOR_W,
            MOTOR_CONNECTOR_D,
            MOTOR_CONNECTOR_H,
            loc=(0, SENSOR_CENTER_Y - MOTOR_BODY_XY / 2.0 - MOTOR_CONNECTOR_D / 2.0, motor_bottom_z + 12.0),
            radius=1.0,
        )
    )
    return motor


def six_compartment_turntable() -> cq.Workplane:
    """Separate six-sector medicine tray with a clearance D-shaped shaft hole."""
    zc = TRAY_BOTTOM_Z + TRAY_H / 2.0
    tray = cylinder_z(TRAY_D, TRAY_FLOOR_H, loc=(0, SENSOR_CENTER_Y, TRAY_BOTTOM_Z + TRAY_FLOOR_H / 2.0))

    rim = cylinder_z(TRAY_D, TRAY_H, loc=(0, SENSOR_CENTER_Y, zc)).cut(
        cylinder_z(TRAY_D - 2 * TRAY_RIM_W, TRAY_H + 2.0, loc=(0, SENSOR_CENTER_Y, zc))
    )
    tray = tray.union(rim)

    # Six radial dividers, producing six 60-degree sector compartments.
    divider_len = TRAY_D / 2.0 - TRAY_RIM_W - TRAY_HUB_D / 2.0
    for angle_deg in range(0, 360, 60):
        # Build each divider on the +X radial line, then rotate it into place.
        # This keeps the divider center and its long axis on the same final ray.
        cx = TRAY_HUB_D / 2.0 + divider_len / 2.0
        cy = SENSOR_CENTER_Y
        divider = rounded_box(
            divider_len,
            TRAY_DIVIDER_W,
            TRAY_H,
            loc=(cx, cy, zc),
            radius=0.4,
        ).rotate((0, SENSOR_CENTER_Y, 0), (0, SENSOR_CENTER_Y, 1), angle_deg)
        tray = tray.union(divider)

    # Central hub around the D-hole.
    tray = tray.union(cylinder_z(TRAY_HUB_D, TRAY_H, loc=(0, SENSOR_CENTER_Y, zc)))

    d_hole = d_profile_z(
        TRAY_D_HOLE_D,
        TRAY_D_HOLE_FLAT_TO_ROUND,
        TRAY_H + 4.0,
        loc=(0, SENSOR_CENTER_Y, zc),
    )
    tray = tray.cut(d_hole)
    return tray


def _point_in_lid_pickout_sector(x: float, y: float, margin: float = 7.0) -> bool:
    """Return True when a point would collide with the lid pick-out opening."""
    dx = x
    dy = y - SENSOR_CENTER_Y
    r = math.hypot(dx, dy)
    if r > LID_PICKOUT_R + margin:
        return False
    angle = math.degrees(math.atan2(dy, dx))
    center = LID_PICKOUT_CENTER_ANGLE_DEG
    delta = (angle - center + 180.0) % 360.0 - 180.0
    return abs(delta) <= LID_PICKOUT_ANGLE_DEG / 2.0 + 6.0


def top_lid_local() -> cq.Workplane:
    """Separate printable top lid with sector pick-out opening and observation holes.

    Local printable coordinates: underside skirt bottom is at Z=0, top face is
    at Z=LID_TOTAL_H. Use top_lid_for_assembly() for the physically stacked
    placement on the base.
    """
    plate_z = LID_SKIRT_H + LID_RAISE_H + LID_PLATE_TH / 2.0
    lid = rounded_box(
        OUTER_X,
        OUTER_Y,
        LID_PLATE_TH,
        loc=(0, 0, plate_z),
        radius=CORNER_R,
    )

    # 10 mm perimeter riser: keeps the original insert/skirt while lifting the
    # top plate to create more clearance above the rotating tray.
    riser_outer = rounded_box(
        OUTER_X,
        OUTER_Y,
        LID_RAISE_H + 2 * LID_INTERNAL_OVERLAP,
        loc=(0, 0, LID_SKIRT_H + LID_RAISE_H / 2.0),
        radius=CORNER_R,
    )
    riser_inner = rounded_box(
        OUTER_X - 2 * WALL,
        OUTER_Y - 2 * WALL,
        LID_RAISE_H + 2.0 + 2 * LID_INTERNAL_OVERLAP,
        loc=(0, 0, LID_SKIRT_H + LID_RAISE_H / 2.0),
        radius=max(CORNER_R - WALL, 1.0),
    )
    lid = lid.union(riser_outer.cut(riser_inner))

    # Downward internal skirt/friction lip that drops inside the open base rim.
    skirt_outer_x = OUTER_X - 2 * WALL - LID_FIT_CLEARANCE
    skirt_outer_y = OUTER_Y - 2 * WALL - LID_FIT_CLEARANCE
    skirt_outer = rounded_box(
        skirt_outer_x,
        skirt_outer_y,
        LID_SKIRT_H + LID_INTERNAL_OVERLAP,
        loc=(0, 0, (LID_SKIRT_H + LID_INTERNAL_OVERLAP) / 2.0),
        radius=max(CORNER_R - WALL - LID_FIT_CLEARANCE / 2.0, 1.0),
    )
    skirt_inner = rounded_box(
        skirt_outer_x - 2 * LID_SKIRT_WALL,
        skirt_outer_y - 2 * LID_SKIRT_WALL,
        LID_SKIRT_H + 2.0,
        loc=(0, 0, LID_SKIRT_H / 2.0),
        radius=max(CORNER_R - WALL - LID_SKIRT_WALL, 1.0),
    )
    lid = lid.union(skirt_outer.cut(skirt_inner))

    # Hidden shoulder that connects the inset friction skirt to the raised
    # perimeter wall, while keeping the center open.
    connector_outer = rounded_box(
        OUTER_X,
        OUTER_Y,
        LID_SKIRT_CONNECTOR_TH,
        loc=(0, 0, LID_SKIRT_H),
        radius=CORNER_R,
    )
    connector_inner = rounded_box(
        skirt_outer_x - 2 * LID_SKIRT_WALL,
        skirt_outer_y - 2 * LID_SKIRT_WALL,
        LID_SKIRT_CONNECTOR_TH + 2.0,
        loc=(0, 0, LID_SKIRT_H),
        radius=max(CORNER_R - WALL - LID_SKIRT_WALL, 1.0),
    )
    lid = lid.union(connector_outer.cut(connector_inner))

    # One top pick-out opening, slightly larger than one 1/6 tray sector.
    top_cut_z = LID_SKIRT_H + LID_RAISE_H + LID_PLATE_TH / 2.0
    lid = lid.cut(
        rounded_sector_prism_z(
            LID_PICKOUT_R,
            LID_PICKOUT_ANGLE_DEG,
            LID_PICKOUT_CORNER_R,
            LID_PLATE_TH + 3.0,
            loc=(0, SENSOR_CENTER_Y, top_cut_z),
            center_angle_deg=LID_PICKOUT_CENTER_ANGLE_DEG,
        )
    )

    # Small evenly distributed observation holes across the remaining lid area.
    for x in LID_OBSERVE_HOLE_XS:
        for y in LID_OBSERVE_HOLE_YS:
            if _point_in_lid_pickout_sector(x, y):
                continue
            lid = lid.cut(cylinder_z(LID_OBSERVE_HOLE_D, LID_PLATE_TH + 3.0, loc=(x, y, top_cut_z)))

    # Light chamfers on top edges: outer rim, observation holes, and sector opening.
    try:
        lid = lid.faces(">Z").edges().chamfer(LID_CHAMFER)
    except Exception:
        pass
    try:
        lid = lid.faces("<Z").edges().chamfer(0.35)
    except Exception:
        pass
    return lid


def top_lid_for_assembly() -> cq.Workplane:
    """Top lid positioned in the real assembly, with skirt entering the base."""
    return top_lid_local().translate((0, 0, BASE_H - LID_SKIRT_H))


def rear_hardware_divider() -> cq.Workplane:
    """Integral rear divider wall with one side top wiring groove."""
    span_x = OUTER_X - 2 * WALL
    wall = rounded_box(
        span_x,
        REAR_DIVIDER_T,
        REAR_DIVIDER_H,
        loc=(0, REAR_DIVIDER_Y, WALL + REAR_DIVIDER_H / 2.0),
        radius=0,
    )

    # Right-side through-hole near the inner side wall for wiring.
    notch_x = OUTER_X / 2.0 - WALL - REAR_DIVIDER_NOTCH_W / 2.0
    notch = rounded_box(
        REAR_DIVIDER_NOTCH_W,
        REAR_DIVIDER_T + 2.0,
        REAR_DIVIDER_NOTCH_H,
        loc=(
            notch_x,
            REAR_DIVIDER_Y,
            REAR_DIVIDER_HOLE_CENTER_Z,
        ),
        radius=1.0,
    )
    return wall.cut(notch)


def screen_mount_hole_cuts() -> list[cq.Workplane]:
    """Four 3.2 mm right-side screw holes matching the serial screen drawing."""
    cuts = []
    for y in SCREEN_MOUNT_HOLE_YS:
        for z in SCREEN_MOUNT_HOLE_ZS:
            cuts.append(
                cylinder_x(
                    SCREEN_MOUNT_HOLE_D,
                    WALL + 12.0,
                    loc=(OUTER_X / 2.0, y, z),
                )
            )
    return cuts


def side_round_hole_cuts() -> list[cq.Workplane]:
    """Six evenly distributed 20 mm circular holes on both left and right side walls."""
    cuts = []
    for side in (-1, 1):
        x = side * (OUTER_X / 2.0 + 0.5)
        for y in SIDE_ROUND_HOLE_YS:
            for z in SIDE_ROUND_HOLE_ZS:
                cuts.append(cylinder_x(SIDE_ROUND_HOLE_D, WALL + 12.0, loc=(x, y, z)))
    return cuts


def serial_screen_placeholder() -> cq.Workplane:
    """Separate simplified TJC8048X270_011R screen placeholder outside the right wall."""
    # Main PCB/envelope sits outside the right wall; the base now only has
    # four matching screw holes, not a large side cutout or protruding frame.
    module_center_x = SCREEN_OUTER_FACE_X + SCREEN_TOTAL_T / 2.0
    screen = rounded_box(
        SCREEN_TOTAL_T,
        SCREEN_PCB_L,
        SCREEN_PCB_H,
        loc=(module_center_x, SCREEN_CENTER_Y, SCREEN_CENTER_Z),
        radius=3.0,
    )

    # Raised LCD iron frame on the visible outside face.
    frame = rounded_box(
        1.2,
        SCREEN_LCD_FRAME_L,
        SCREEN_LCD_FRAME_H,
        loc=(SCREEN_OUTER_FACE_X + SCREEN_TOTAL_T + 0.6, SCREEN_CENTER_Y, SCREEN_CENTER_Z),
        radius=0.4,
    )
    screen = screen.union(frame)

    # Slightly raised visible active area for scale/readability.
    active = rounded_box(
        1.6,
        SCREEN_LCD_AA_L,
        SCREEN_LCD_AA_H,
        loc=(SCREEN_OUTER_FACE_X + SCREEN_TOTAL_T + 1.1, SCREEN_CENTER_Y, SCREEN_CENTER_Z),
        radius=0.3,
    )
    screen = screen.union(active)

    # Matching through-holes in the PCB placeholder make alignment easy to see.
    for y in SCREEN_MOUNT_HOLE_YS:
        for z in SCREEN_MOUNT_HOLE_ZS:
            screen = screen.cut(
                cylinder_x(
                    SCREEN_MOUNT_HOLE_D,
                    SCREEN_TOTAL_T + 4.0,
                    loc=(module_center_x, y, z),
                )
            )

    # Small rear connector bump from the drawing, simplified on the inner side.
    connector = rounded_box(
        6.0,
        15.0,
        14.85,
        loc=(SCREEN_OUTER_FACE_X - 3.0, 43.21, SCREEN_CENTER_Z - SCREEN_PCB_H / 2.0 + 21.45),
        radius=0.8,
    )
    screen = screen.union(connector)
    return screen


def base_v1() -> cq.Workplane:
    """Return the minimal base body as a single printable solid."""
    base = rounded_box(
        OUTER_X,
        OUTER_Y,
        BASE_H,
        loc=(0, 0, BASE_H / 2.0),
        radius=CORNER_R,
    )

    # Open-top hollow interior: keep only floor + perimeter walls.
    cavity_h = BASE_H - WALL + 4.0
    cavity = rounded_box(
        OUTER_X - 2 * WALL,
        OUTER_Y - 2 * WALL,
        cavity_h,
        loc=(0, 0, WALL + cavity_h / 2.0),
        radius=max(CORNER_R - WALL, 1.0),
    )
    base = base.cut(cavity)

    # Simple QR100 front wall opening only; no frame, clamps, step, tunnel, or bezel.
    qr_cut = rounded_box(
        QR_OPENING_W,
        WALL + 6.0,
        QR_OPENING_H,
        loc=(0, FRONT_Y + WALL / 2.0, QR_CENTER_Z),
        radius=QR_CORNER_R,
    )
    base = base.cut(qr_cut)

    # Recessed branding on the upper rear wall.
    base = base.cut(rear_logo_engraving_cut())

    # Rear cable pass-through: centered on the back wall.
    cable_cut = cylinder_y(
        CABLE_HOLE_D,
        WALL + 8.0,
        loc=(0, OUTER_Y / 2.0 + 1.0, CABLE_CENTER_Z),
    )
    base = base.cut(cable_cut)

    # Right-side serial screen: no frame and no large opening, only four
    # drawing-matched screw holes for fixing the external module.
    for screw_hole in screen_mount_hole_cuts():
        base = base.cut(screw_hole)

    # Integral retaining frame for the separate simplified 100 mm sensor cylinder.
    base = base.union(sensor_retainer_ring())

    # Rear hardware/wiring isolation wall with one side wiring notch.
    base = base.union(rear_hardware_divider())

    return base


def base_lightweight() -> cq.Workplane:
    """Printable lighter base with rounded side-wall cutouts in low-risk areas."""
    base = base_v1()

    # Both long sides: 6 evenly distributed 20 mm circular holes.
    # Right side also keeps the four small screen mounting holes from base_v1().
    for side_hole in side_round_hole_cuts():
        base = base.cut(side_hole)

    # Rear wall lightening slots placed left/right of the existing cable hole.
    rear_slot_w = 26.0
    rear_slot_h = 34.0
    for x in (-45.0, 45.0):
        cut = rounded_box(rear_slot_w, WALL + 8.0, rear_slot_h - 12.0, loc=(x, OUTER_Y / 2.0 + 0.5, 62.0), radius=0)
        cut = cut.union(cylinder_y(12.0, WALL + 8.0, loc=(x, OUTER_Y / 2.0 + 0.5, 62.0 + (rear_slot_h - 12.0) / 2.0)))
        cut = cut.union(cylinder_y(12.0, WALL + 8.0, loc=(x, OUTER_Y / 2.0 + 0.5, 62.0 - (rear_slot_h - 12.0) / 2.0)))
        base = base.cut(cut)

    return base


def gen_step():
    """CAD skill entry point."""
    return base_v1()


def assembly_with_sensor_placeholder() -> cq.Assembly:
    """Preview assembly: base and separate movable sensor placeholder."""
    return (
        cq.Assembly(name="smart_pillbox_base_with_sensor_placeholder")
        .add(base_v1(), name="base_integral_retainer")
        .add(qr100_scanner_placeholder(), name="separate_zy_qr100_scanner")
        .add(hx711_sensor_placeholder(), name="separate_hx711_sensor_cylinder")
        .add(motor_mount_placeholder(), name="separate_downward_open_motor_mount")
        .add(stepper_motor_placeholder(), name="separate_nema17_stepper_motor")
        .add(six_compartment_turntable(), name="separate_six_compartment_turntable")
        .add(serial_screen_placeholder(), name="separate_tjc8048x270_serial_screen")
    )


def assembly_lightweight_with_placeholders() -> cq.Assembly:
    """Preview assembly using the lightweight printable base and separate placeholders."""
    return (
        cq.Assembly(name="smart_pillbox_base_lightweight_with_placeholders")
        .add(base_lightweight(), name="base_lightweight_integral_retainer")
        .add(qr100_scanner_placeholder(), name="separate_zy_qr100_scanner")
        .add(hx711_sensor_placeholder(), name="separate_hx711_sensor_cylinder")
        .add(motor_mount_placeholder(), name="separate_downward_open_motor_mount")
        .add(stepper_motor_placeholder(), name="separate_nema17_stepper_motor")
        .add(six_compartment_turntable(), name="separate_six_compartment_turntable")
        .add(serial_screen_placeholder(), name="separate_tjc8048x270_serial_screen")
        .add(top_lid_for_assembly(), name="separate_top_lid_with_pickout_and_observation_holes")
    )


def export(outdir: Path | None = None) -> Path:
    """Export STEP and STL sidecars for quick local preview/printing."""
    if outdir is None:
        outdir = Path(__file__).resolve().parent / "output"
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / "stl").mkdir(exist_ok=True)

    base = base_v1()
    exporters.export(base, str(outdir / "smart_pillbox_base.step"), exportType="STEP")
    exporters.export(
        base,
        str(outdir / "stl" / "smart_pillbox_base.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    light_base = base_lightweight()
    exporters.export(light_base, str(outdir / "smart_pillbox_base_lightweight.step"), exportType="STEP")
    exporters.export(
        light_base,
        str(outdir / "smart_pillbox_base_lightweight_with_screen_mount_holes.step"),
        exportType="STEP",
    )
    exporters.export(
        light_base,
        str(outdir / "stl" / "smart_pillbox_base_lightweight.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    exporters.export(
        light_base,
        str(outdir / "stl" / "smart_pillbox_base_lightweight_with_screen_mount_holes.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    sensor = hx711_sensor_placeholder()
    exporters.export(
        sensor,
        str(outdir / "hx711_sensor_placeholder.step"),
        exportType="STEP",
    )
    exporters.export(
        sensor,
        str(outdir / "stl" / "hx711_sensor_placeholder.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    qr100 = qr100_scanner_placeholder()
    exporters.export(
        qr100,
        str(outdir / "zy_qr100_scanner_placeholder.step"),
        exportType="STEP",
    )
    exporters.export(
        qr100,
        str(outdir / "stl" / "zy_qr100_scanner_placeholder.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    mount = motor_mount_placeholder()
    exporters.export(
        mount,
        str(outdir / "motor_mount_placeholder.step"),
        exportType="STEP",
    )
    exporters.export(
        mount,
        str(outdir / "stl" / "motor_mount_placeholder.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    motor = stepper_motor_placeholder()
    exporters.export(
        motor,
        str(outdir / "stepper_motor_placeholder.step"),
        exportType="STEP",
    )
    exporters.export(
        motor,
        str(outdir / "stl" / "stepper_motor_placeholder.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    tray = six_compartment_turntable()
    exporters.export(
        tray,
        str(outdir / "six_compartment_turntable.step"),
        exportType="STEP",
    )
    exporters.export(
        tray,
        str(outdir / "stl" / "six_compartment_turntable.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    lid = top_lid_local()
    exporters.export(
        lid,
        str(outdir / "smart_pillbox_top_lid.step"),
        exportType="STEP",
    )
    exporters.export(
        lid,
        str(outdir / "stl" / "smart_pillbox_top_lid.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    screen = serial_screen_placeholder()
    exporters.export(
        screen,
        str(outdir / "tjc8048x270_serial_screen_placeholder.step"),
        exportType="STEP",
    )
    exporters.export(
        screen,
        str(outdir / "stl" / "tjc8048x270_serial_screen_placeholder.stl"),
        exportType="STL",
        tolerance=0.15,
        angularTolerance=0.15,
    )
    assembly_with_sensor_placeholder().save(
        str(outdir / "smart_pillbox_base_with_sensor_placeholder.step"),
        exportType="STEP",
    )
    assembly_lightweight_with_placeholders().save(
        str(outdir / "smart_pillbox_base_lightweight_with_placeholders.step"),
        exportType="STEP",
    )
    assembly_lightweight_with_placeholders().save(
        str(outdir / "smart_pillbox_base_lightweight_with_screen_mount_holes_assembly.step"),
        exportType="STEP",
    )
    return outdir


if __name__ == "__main__":
    print(export())
