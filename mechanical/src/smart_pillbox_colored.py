"""Presentation-ready colored assembly for the smart pillbox prototype.

Units: millimeters. Geometry is imported from base_v1_minimal.py; this file only
adds CAD assembly colors and a few thin visual overlays so the model reads better
in product/report renders without changing printable part geometry.
"""
from __future__ import annotations

from pathlib import Path

import cadquery as cq

import base_v1_minimal as b


def C(hex_color: str, alpha: float = 1.0) -> cq.Color:
    """Create a CadQuery color from #RRGGBB."""
    h = hex_color.strip().lstrip("#")
    r = int(h[0:2], 16) / 255.0
    g = int(h[2:4], 16) / 255.0
    bl = int(h[4:6], 16) / 255.0
    return cq.Color(r, g, bl, alpha)


# Soft product palette: warm shell, calm tech accent, dark electronics.
COLORS = {
    "base": C("F2F0EA"),          # warm white printable shell
    "lid": C("DDEBFF", 0.72),     # pale blue translucent-feeling cover
    "tray": C("2EC4B6"),          # hygienic teal medicine tray
    "sensor": C("CBD5E1"),        # light metal load-cell/sensor stack
    "motor_mount": C("F59E0B"),   # amber mechanical adapter
    "motor": C("374151"),         # graphite stepper motor body
    "shaft": C("A3A3A3"),         # brushed metal shaft visual overlay
    "qr": C("111827"),            # dark QR scanner module
    "qr_glass": C("7DD3FC", 0.85),
    "screen_body": C("0F172A"),   # black screen PCB/enclosure
    "screen_glass": C("1D4ED8", 0.88),
    "screen_frame": C("020617"),
}


def shaft_visual_overlay() -> cq.Workplane:
    """Separate metallic D-shaft overlay for clearer renders."""
    top_z = b.MOTOR_MOUNT_TOP_Z + b.MOTOR_BODY_H + b.MOTOR_FLANGE_TH + b.MOTOR_BOSS_H
    return b.d_shaft_z(
        b.MOTOR_SHAFT_D + 0.08,
        b.MOTOR_SHAFT_H,
        loc=(0, b.SENSOR_CENTER_Y, top_z + b.MOTOR_SHAFT_H / 2.0),
    )


def qr100_scan_window_glass() -> cq.Workplane:
    """Thin cyan insert on the QR100 front face so the scanner reads as optical."""
    front_face_y = b.FRONT_Y - b.QR100_FRONT_T
    return b.rounded_box(
        b.QR100_SCAN_WINDOW_W - 2.0,
        0.65,
        b.QR100_SCAN_WINDOW_H - 2.0,
        loc=(0, front_face_y - 0.35, b.QR_CENTER_Z),
        radius=0,
    )


def screen_active_glass() -> cq.Workplane:
    """Thin blue active-display overlay on the outside face of the serial screen."""
    return b.rounded_box(
        0.9,
        b.SCREEN_LCD_AA_L,
        b.SCREEN_LCD_AA_H,
        loc=(b.SCREEN_OUTER_FACE_X + b.SCREEN_TOTAL_T + 1.85, b.SCREEN_CENTER_Y, b.SCREEN_CENTER_Z),
        radius=0,
    )


def screen_dark_frame_overlay() -> cq.Workplane:
    """Dark bezel overlay around the active display for a more realistic HMI look."""
    outer = b.rounded_box(
        0.8,
        b.SCREEN_LCD_FRAME_L,
        b.SCREEN_LCD_FRAME_H,
        loc=(b.SCREEN_OUTER_FACE_X + b.SCREEN_TOTAL_T + 1.55, b.SCREEN_CENTER_Y, b.SCREEN_CENTER_Z),
        radius=0,
    )
    inner = b.rounded_box(
        1.2,
        b.SCREEN_LCD_AA_L + 1.5,
        b.SCREEN_LCD_AA_H + 1.5,
        loc=(b.SCREEN_OUTER_FACE_X + b.SCREEN_TOTAL_T + 1.55, b.SCREEN_CENTER_Y, b.SCREEN_CENTER_Z),
        radius=0,
    )
    return outer.cut(inner)


def colored_assembly() -> cq.Assembly:
    """Full colored smart pillbox assembly for review screenshots and PPT."""
    asm = cq.Assembly(name="smart_pillbox_colored_product_render")
    asm.add(b.base_lightweight(), name="warm_white_printed_base", color=COLORS["base"])
    asm.add(b.hx711_sensor_placeholder(), name="silver_hx711_load_sensor_cylinder", color=COLORS["sensor"])
    asm.add(b.motor_mount_placeholder(), name="amber_downward_open_motor_mount", color=COLORS["motor_mount"])
    asm.add(b.stepper_motor_placeholder(), name="graphite_nema17_stepper_motor", color=COLORS["motor"])
    asm.add(shaft_visual_overlay(), name="metallic_d_shaft_visual_overlay", color=COLORS["shaft"])
    asm.add(b.six_compartment_turntable(), name="teal_six_compartment_medicine_tray", color=COLORS["tray"])
    asm.add(b.top_lid_for_assembly(), name="pale_blue_raised_top_lid", color=COLORS["lid"])
    asm.add(b.qr100_scanner_placeholder(), name="black_zy_qr100_scanner", color=COLORS["qr"])
    asm.add(qr100_scan_window_glass(), name="cyan_qr100_scan_window", color=COLORS["qr_glass"])
    asm.add(b.serial_screen_placeholder(), name="dark_tjc8048x270_serial_screen", color=COLORS["screen_body"])
    asm.add(screen_dark_frame_overlay(), name="black_screen_bezel_overlay", color=COLORS["screen_frame"])
    asm.add(screen_active_glass(), name="blue_active_display_overlay", color=COLORS["screen_glass"])
    return asm


def colored_internal_assembly() -> cq.Assembly:
    """Same color scheme without the top lid, useful for reports showing the stack."""
    asm = cq.Assembly(name="smart_pillbox_colored_internal_no_lid")
    asm.add(b.base_lightweight(), name="warm_white_printed_base", color=COLORS["base"])
    asm.add(b.hx711_sensor_placeholder(), name="silver_hx711_load_sensor_cylinder", color=COLORS["sensor"])
    asm.add(b.motor_mount_placeholder(), name="amber_downward_open_motor_mount", color=COLORS["motor_mount"])
    asm.add(b.stepper_motor_placeholder(), name="graphite_nema17_stepper_motor", color=COLORS["motor"])
    asm.add(shaft_visual_overlay(), name="metallic_d_shaft_visual_overlay", color=COLORS["shaft"])
    asm.add(b.six_compartment_turntable(), name="teal_six_compartment_medicine_tray", color=COLORS["tray"])
    asm.add(b.qr100_scanner_placeholder(), name="black_zy_qr100_scanner", color=COLORS["qr"])
    asm.add(qr100_scan_window_glass(), name="cyan_qr100_scan_window", color=COLORS["qr_glass"])
    asm.add(b.serial_screen_placeholder(), name="dark_tjc8048x270_serial_screen", color=COLORS["screen_body"])
    asm.add(screen_dark_frame_overlay(), name="black_screen_bezel_overlay", color=COLORS["screen_frame"])
    asm.add(screen_active_glass(), name="blue_active_display_overlay", color=COLORS["screen_glass"])
    return asm


def gen_step():
    """CAD skill entry point: full colored assembly."""
    return colored_assembly()


def export(outdir: Path | None = None) -> Path:
    if outdir is None:
        outdir = Path(__file__).resolve().parent / "output"
    outdir.mkdir(parents=True, exist_ok=True)

    colored_assembly().save(
        str(outdir / "smart_pillbox_colored_product_render.step"),
        exportType="STEP",
    )
    colored_internal_assembly().save(
        str(outdir / "smart_pillbox_colored_internal_no_lid.step"),
        exportType="STEP",
    )
    return outdir


if __name__ == "__main__":
    print(export())
