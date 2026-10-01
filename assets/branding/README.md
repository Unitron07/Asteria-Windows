# Asteria artwork

- `asteria-logo.png`: untouched canonical main/large mark, used at the top of the repository README.
- `asteria-icon-small-master.png`: untouched approved simplified artwork for small icons (the supplied “Asteria 48px.png”; the source itself is full resolution).

Run `python scripts/generate-asteria-icons.py` from any directory with Pillow 12.3.0 installed. The generated `app/asteria.ico` contains 16, 20, 24, 32, 40, 48, 64, 128 and 256 px frames. Sizes through 64 px use the small master; 128/256 px use the main mark. Lanczos resizing preserves the full square, padding, RGB colors and intentional opaque black background. Do not replace either canonical source with a downscaled file.

`app/asteria-wix.png` and `app/res/asteria.svg` are generated from the 64 px small-master image. The SVG embeds that PNG for the inherited QSvgRenderer window-icon consumer; the Qt resource alias `res/moonlight.svg` remains compatible so Session code need not change. The application window uses the multi-resolution ICO directly.

Windows x64 and ARM64 share `RC_ICONS = asteria.ico`; portable packaging copies the resulting executable and installer shortcuts target it. The inherited WiX bootstrapper also uses the Asteria ICO and 64 px logo. Original Moonlight SVG/ICNS/Steam Link artwork remains for inherited non-Windows packaging; Moonlight attribution, licenses and history remain intact.
