from pathlib import Path

from PIL import Image, ImageChops


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware/assets/logo-speed-bike.png"
OUTPUT = ROOT / "firmware/src/assets/speedbike_logo.h"
BACKGROUND = (2, 8, 15)
MAX_SIZE = (296, 200)


def rgb565_bytes(red: int, green: int, blue: int) -> bytes:
    value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
    return bytes((value >> 8, value & 0xFF))


def crop_image(image: Image.Image) -> Image.Image:
    alpha = image.getchannel("A")
    background = Image.new("RGBA", image.size, (*BACKGROUND, 255))
    difference = ImageChops.difference(image, background)
    mask = ImageChops.lighter(alpha, difference.convert("L"))
    bounds = mask.getbbox()
    return image.crop(bounds) if bounds else image


def main() -> None:
    image = Image.open(SOURCE).convert("RGBA")
    image = crop_image(image)
    image.thumbnail(MAX_SIZE, Image.Resampling.LANCZOS)
    flattened = Image.new("RGB", image.size, BACKGROUND)
    flattened.paste(image, mask=image.getchannel("A"))
    pixels = b"".join(rgb565_bytes(*pixel) for pixel in flattened.getdata())
    lines = [", ".join(f"0x{byte:02x}" for byte in pixels[index:index + 16]) for index in range(0, len(pixels), 16)]
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(
        "#pragma once\n\n#include <Arduino.h>\n\n"
        f"constexpr uint16_t SpeedbikeLogoWidth = {flattened.width};\n"
        f"constexpr uint16_t SpeedbikeLogoHeight = {flattened.height};\n"
        "const uint8_t SpeedbikeLogoPixels[] PROGMEM = {\n    "
        + ",\n    ".join(lines)
        + "\n};\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
