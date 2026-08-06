"""CAD Viewer generator for current lightweight smart pillbox assembly."""
from __future__ import annotations

from pathlib import Path
import sys

THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

from base_v1_minimal import assembly_lightweight_with_placeholders


def gen_step():
    return assembly_lightweight_with_placeholders()
