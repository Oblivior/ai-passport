"""Ensure checked-in subsets cover every non-ASCII UI literal."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class UIFontTests(unittest.TestCase):
    def test_fixed_ui_copy_has_both_font_sizes(self):
        chars = set()
        for path in sorted((ROOT / "main").glob("*.c")):
            if not re.fullmatch(r"(demo_.*|main|pet_ui_text|pet_catalog|pet_meet)", path.stem):
                continue
            for literal in re.findall(r'"(?:[^"\\]|\\.)*"', path.read_text()):
                chars.update(c for c in literal if ord(c) > 127)
        self.assertGreater(len(chars), 100)
        for size in (14, 20):
            source = (ROOT / f"assets/fonts/passport_zh_{size}.c").read_text()
            glyphs = {int(code, 16) for code in re.findall(r"/\* U\+([0-9A-Fa-f]+)", source)}
            missing = sorted(c for c in chars if ord(c) not in glyphs)
            self.assertEqual(missing, [], f"{size}px needs regeneration: {missing}")
            self.assertTrue(set(range(32, 127)) <= glyphs)
            self.assertNotIn("/Users/", source)
            self.assertNotIn("/home/", source)


if __name__ == "__main__":
    unittest.main()
