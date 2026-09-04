import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GUARD = ROOT / "tools" / "check_source_only.py"


class SourceOnlyGuardTest(unittest.TestCase):
    def run_guard(self, root: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(GUARD), str(root)],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_only_root_generated_directories_are_skipped(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "build").mkdir()
            (root / "build" / "generated.exe").touch()
            allowed = self.run_guard(root)
            self.assertEqual(allowed.returncode, 0, allowed.stderr)

            nested = root / "docs" / "build"
            nested.mkdir(parents=True)
            payload = nested / "payload.exe"
            payload.touch()
            rejected = self.run_guard(root)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn(str(payload), rejected.stderr)


if __name__ == "__main__":
    unittest.main()
