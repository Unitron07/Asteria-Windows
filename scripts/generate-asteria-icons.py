#!/usr/bin/env python3
"""Generate Asteria Windows/Qt assets from the approved, untouched PNG masters.

Requires Pillow 12.3.0 for byte-for-byte reproducibility.
"""
import base64
import io
from pathlib import Path
import struct

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def png_bytes(image):
    output = io.BytesIO()
    image.save(output, format="PNG", compress_level=9)
    return output.getvalue()


def crop_emblem(source, bounds):
    """Keep the circular black interior; remove only the rectangular exterior."""
    display = source.crop(bounds).convert("RGBA")
    scale = 4
    mask = Image.new("L", (display.width * scale, display.height * scale), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, mask.width - 1, mask.height - 1), fill=255)
    display.putalpha(mask.resize(display.size, Image.Resampling.LANCZOS))
    return display


def main():
    branding = ROOT / "assets/branding"
    with Image.open(branding / "asteria-icon-small-master.png") as source:
        small = source.convert("RGB")
    with Image.open(branding / "asteria-logo.png") as source:
        large = source.convert("RGB")
    for source in (small, large):
        if source.width != source.height:
            raise ValueError("Approved masters must be square; do not crop artwork")

    # Display-only crop around the approved main mark's outer ring/glow. Remove
    # rectangular canvas corners, not the black circular interior or RGB artwork.
    # Coordinates apply to the untouched 1254 px approved master.
    if large.size != (1254, 1254) or small.size != (1254, 1254):
        raise ValueError("Review the crop coordinates if an approved master changes")
    display = crop_emblem(large, (52, 56, 1192, 1172))
    (branding / "asteria-logo-readme.png").write_bytes(png_bytes(display))

    # Apply the same tight circular crop to the simplified artwork. Bounds follow
    # its thicker ring/glow. Transparent square padding preserves its aspect ratio
    # for Windows ICO frames; the black circular interior stays opaque.
    cropped_small = crop_emblem(small, (48, 54, 1209, 1174))
    edge = max(cropped_small.size)
    icon = Image.new("RGBA", (edge, edge), (0, 0, 0, 0))
    icon.paste(cropped_small, ((edge - cropped_small.width) // 2,
                              (edge - cropped_small.height) // 2))

    # Masters are untagged sRGB artwork. Keep RGB values; no palette quantization
    # or sharpening. Every shell resolution uses only the simplified master.
    frames = {}
    for size in SIZES:
        frames[size] = png_bytes(icon.resize((size, size), Image.Resampling.LANCZOS))

    # Write individual PNG-compressed ICO entries, rather than allowing an ICO
    # encoder to derive every resolution from a single large image.
    offset = 6 + 16 * len(SIZES)
    entries = []
    for size in SIZES:
        data = frames[size]
        entries.append(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    (ROOT / "app/asteria.ico").write_bytes(
        struct.pack("<HHH", 0, 1, len(SIZES)) + b"".join(entries) + b"".join(frames.values())
    )
    (ROOT / "app/asteria-wix.png").write_bytes(frames[64])

    # QSvgRenderer is used by the inherited SDL window icon code. Keep its
    # resource alias working with an embedded small-master PNG, without changing
    # Session code. This SVG is a raster container, not recreated vector artwork.
    encoded = base64.b64encode(frames[64]).decode("ascii")
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" '
           'xmlns:xlink="http://www.w3.org/1999/xlink" width="64" height="64" viewBox="0 0 64 64">\n'
           f'  <image width="64" height="64" xlink:href="data:image/png;base64,{encoded}"/>\n'
           '</svg>\n')
    (ROOT / "app/res/asteria.svg").write_text(svg, encoding="utf-8", newline="\n")
    print("Generated Asteria ICO:", ", ".join(f"{s}x{s}" for s in SIZES))


if __name__ == "__main__":
    main()
