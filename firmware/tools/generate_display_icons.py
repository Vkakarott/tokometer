"""Convert the shared provider logos to small, four-level alpha masks for the TFT."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "macos/Sources/TokEspApp/Resources"
DESTINATION = ROOT / "firmware/src/assets/provider_icons.h"
SIZE = 22

lines = ["#pragma once", "", "#include <cstdint>", "", "namespace ProviderIcons {",
         f"constexpr uint8_t Size = {SIZE};"]
for name in ("codex", "claude", "cursor"):
    alpha = Image.open(SOURCE / f"{name}.png").convert("RGBA").getchannel("A")
    alpha = alpha.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    values = [min(3, (value + 42) // 85) for value in alpha.getdata()]
    packed = [sum(values[i + j] << (6 - 2 * j) for j in range(4)) for i in range(0, len(values), 4)]
    lines.append(f"constexpr uint8_t {name.title()}[] = {{")
    for start in range(0, len(packed), 16):
        lines.append("    " + ", ".join(f"0x{v:02x}" for v in packed[start:start + 16]) + ",")
    lines.append("};")
lines.append("}")
DESTINATION.write_text("\n".join(lines) + "\n")
