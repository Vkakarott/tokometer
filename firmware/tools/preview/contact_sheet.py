#!/usr/bin/env python3
"""Compose native renderer output without redrawing any UI elements."""

from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageFont


output_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[3] / ".pio/display-preview"
fixtures = (
    ("codex-5h", "CODEX - 5H"),
    ("claude-5h", "CLAUDE - 5H"),
    ("cursor-plan", "CURSOR - PLANO"),
    ("codex-7d", "CODEX - 7D"),
    ("claude-7d", "CLAUDE - 7D"),
    ("cursor-api", "CURSOR - API"),
)
scale = 4
width, height = 296 * scale, 240 * scale
gutter, label_height = 24, 42
sheet = Image.new("RGB", (3 * width + 4 * gutter, 2 * (height + label_height) + 3 * gutter), "#111820")
draw = ImageDraw.Draw(sheet)
font = ImageFont.load_default(size=24)
for i, (name, label) in enumerate(fixtures):
    x = gutter + (i % 3) * (width + gutter)
    y = gutter + (i // 3) * (height + label_height + gutter)
    draw.text((x, y), label, fill="#dceafa", font=font)
    with Image.open(output_dir / f"{name}.png") as image:
        sheet.paste(image.convert("RGB").resize((width, height), Image.Resampling.NEAREST), (x, y + label_height))
path = output_dir / "dashboard-sheet.png"
sheet.save(path)
print(path)
