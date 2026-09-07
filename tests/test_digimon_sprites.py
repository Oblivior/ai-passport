"""Check generated artwork provenance without requiring Node for the static gate."""
import hashlib
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = root / "tools/generate_digimon_sprites.mjs"
generated = (root / "assets/images/digimon_sprites.c").read_text()
assert "Generator SHA256: " + hashlib.sha256(source.read_bytes()).hexdigest() in generated
arrays = re.findall(r"static const LV_ATTRIBUTE_MEM_ALIGN uint8_t (\w+)\[\] = \{(.*?)\};", generated, re.S)
assert len(arrays) == 63
for name, body in arrays:
    data = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", body))
    assert len(data) == 32 * 28 * 3, name
    alpha = data[32 * 28 * 2:]
    assert set(alpha) == {0, 255}, name
    assert not any(alpha[:32]) and not any(alpha[-32:]), name
    assert not any(alpha[::32]) and not any(alpha[31::32]), name
assert generated.count(".cf = LV_COLOR_FORMAT_RGB565A8") == 63
print("Digimon sprites: PASS (source hash, 3 lines x 7 forms x 3 poses, alpha and bounds)")
