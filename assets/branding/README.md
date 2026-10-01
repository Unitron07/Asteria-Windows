# Asteria artwork

- `asteria-logo.png`: untouched canonical main/large mark, the source for README and large branding.
- `asteria-logo-readme.png`: generated display-only crop of the main mark, used at the top of the repository README. The crop surrounds the ring/glow at source coordinates `(52, 56, 1192, 1172)`; an antialiased elliptical alpha mask removes exterior rectangular black corners while preserving the black circular interior and original RGB artwork.
- `asteria-icon-small-master.png`: untouched approved simplified artwork for small icons (the supplied “Asteria 48px.png”; the source itself is full resolution).

Run `python scripts/generate-asteria-icons.py` from any directory with Pillow 12.3.0 installed. The generated `app/asteria.ico` contains 16, 20, 24, 32, 40, 48, 64, 128 and 256 px frames. Every frame uses the simplified small master, including the large shell/Explorer sizes. The detailed main mark is reserved for README/large branding. Icons use the same tight elliptical crop treatment as README, with bounds `(48, 54, 1209, 1174)` following the simplified mark's thicker ring/glow. Transparent square padding preserves its aspect ratio, and Lanczos resizing preserves RGB colors. The circular black interior stays opaque; only the exterior canvas/corners become transparent. Do not replace either canonical source with a cropped or downscaled file.

`app/asteria-wix.png` and `app/res/asteria.svg` are generated from the 64 px small-master image. The SVG embeds that PNG for the inherited QSvgRenderer window-icon consumer; the Qt resource alias `res/moonlight.svg` remains compatible so Session code need not change. The application window uses the multi-resolution ICO directly.

Windows x64 and ARM64 share `RC_ICONS = asteria.ico`; portable packaging copies the resulting executable and installer shortcuts target it. The inherited WiX bootstrapper also uses the Asteria ICO and 64 px logo. Original Moonlight SVG/ICNS/Steam Link artwork remains for inherited non-Windows packaging; Moonlight attribution, licenses and history remain intact.
