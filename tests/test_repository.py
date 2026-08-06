import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PublicationRepositoryTests(unittest.TestCase):
    def test_required_structure_and_files_exist(self):
        required = [
            "android/app",
            "android/gradle/wrapper",
            "firmware/Core",
            "firmware/Drivers",
            "firmware/include",
            "firmware/src",
            "mechanical/src",
            "mechanical/models",
            "docs/images/product",
            "docs/images/cad",
            "LICENSES",
        ]
        missing = [item for item in required if not (ROOT / item).exists()]
        self.assertEqual([], missing, f"missing publication paths: {missing}")

    def test_required_publication_documents_exist(self):
        required = [
            "README.md",
            "docs/architecture.md",
            "docs/protocol.md",
            "LICENSES/MIT.txt",
            "LICENSES/CERN-OHL-P-2.0.txt",
            "LICENSES/README.md",
        ]
        missing = [item for item in required if not (ROOT / item).is_file()]
        self.assertEqual([], missing, f"missing publication documents: {missing}")

    def test_local_and_generated_artifacts_are_absent(self):
        forbidden_names = {
            "local.properties",
            ".em_skill.json",
            "src_backup",
            ".gradle",
            ".cxx",
            ".pio",
            "__pycache__",
        }
        forbidden_suffixes = {".apk", ".aab", ".jks", ".keystore", ".bak", ".glb"}
        offenders = []
        for path in ROOT.rglob("*"):
            relative = path.relative_to(ROOT)
            if relative.parts[:2] == ("tests", "__pycache__"):
                continue
            if path.name in forbidden_names or path.suffix.lower() in forbidden_suffixes:
                offenders.append(str(relative))
            if path.is_dir() and path.name == "build":
                offenders.append(str(relative))
        self.assertEqual([], offenders, f"forbidden artifacts: {offenders}")

    def test_firmware_has_no_machine_specific_monitor_port(self):
        platformio = (ROOT / "firmware/platformio.ini").read_text(encoding="utf-8")
        self.assertNotRegex(platformio, r"(?im)^\s*monitor_port\s*=")

    def test_readme_states_scope_and_license_split(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("独立开发的课程原型", readme)
        self.assertIn("MIT", readme)
        self.assertIn("CERN-OHL-P-2.0", readme)
        self.assertRegex(readme, re.compile(r"Bluetooth.*(?:SPP|RFCOMM)|(?:SPP|RFCOMM).*Bluetooth", re.I | re.S))

    def test_no_large_files_or_machine_paths(self):
        large = [str(p.relative_to(ROOT)) for p in ROOT.rglob("*") if p.is_file() and p.stat().st_size > 5 * 1024 * 1024]
        self.assertEqual([], large, f"files over 5 MiB: {large}")
        suspicious = []
        patterns = ("local.properties", "COM5", "C:\\\\Users\\", "D:\\\\")
        for path in ROOT.rglob("*"):
            if "tests" in path.relative_to(ROOT).parts or path.name == ".gitignore":
                continue
            if not path.is_file() or path.suffix.lower() in {".png", ".jpg", ".jpeg", ".step", ".stp", ".stl", ".jar"}:
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            if any(pattern in text for pattern in patterns):
                suspicious.append(str(path.relative_to(ROOT)))
        self.assertEqual([], suspicious, f"machine-specific references: {suspicious}")


if __name__ == "__main__":
    unittest.main()
