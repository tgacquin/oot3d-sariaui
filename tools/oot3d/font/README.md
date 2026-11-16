# Redrawn HD lettering

`outlines.py` contains the original hand-drawn curves chosen in the dialogue
preview. HD Thin uses 1.45 native pixels; HD Heavy uses 1.65. Accents
are composed from those curves; no replacement typeface or another mod's font
artwork is used.

Regenerate the checked-in artwork with Python and Pillow:

```sh
python3 tools/oot3d/font/generate.py /path/to/message/us/ltn16.qbf \
  --header src/oot3d/hd_font_data.h --preview /tmp/oot-font-proof
python3 tools/oot3d/font/generate.py /path/to/message/us/ltn16.qbf --weight 1.45 \
  --header src/oot3d/hd_font_thin_data.h --preview /tmp/oot-font-thin-proof
```

The QBF supplies codepoints, crop metrics and recognition hashes. Its bitmap
data is not embedded in the generated header. The output contains 196 visible
characters as run-length encoded 64×64 alpha cells. Normal space, nonbreaking
space and the native blank placeholder retain their existing behavior.

At startup, the host loads native signatures from the user's ROM. The Vulkan
replacement recognizes complete, byte-identical 16×16 glyph cells in verified
QBF uploads and substitutes the new art. UVs, native advances, text color,
geometry, typewriter timing and guest memory are unchanged. Other fonts,
modified QBF glyphs and custom texture replacements remain native. Embedded
unrecognized symbols retain bilinear native coverage.

The custom keyboard, file-status lettering, PRESS START and pause/file/ocarina
button prompts also have host-side 4x RGBA atlases, built once at game load from
the same glyph artwork. Their native cropped positions and one-pixel shadow are
retained. Matching uses the complete native atlas hash and dimensions; only the
known generated text atlases qualify. Guest images, meshes, UVs, layouts and
button-icon art are unchanged. The HD toggle selects the host view immediately.
Decorative text baked into game textures (ITEMS, GEAR, etc.) stays original.

File-stone dates and times use a separate 8×16 IA4 system font, including a
baked dark outline. `generate_system.py` adapts our original curves to those
native metrics and generates 17 outlined RGBA glyphs (digits, date/time
separators, and AM/PM). The existing HD font toggle controls these too.
Recognition checks the complete native cell, including luminance and alpha;
unsupported symbols and modified font cells retain their native appearance.
The replacement is host-only and adds no guest texture allocations.

```sh
python3 tools/oot3d/font/generate_system.py /path/to/message/sys8.qbf \
  --header src/oot3d/hd_system_font_data.h --preview /tmp/oot-hd-time-proof.png
```

`oot3d_hd_font_check` executes the game's unmodified ARM A4 upload routine for
all 196 glyphs and checks the decoded result, orientation, full HD coverage,
adjacent cells, A8 decoding, unknown symbols, malformed/custom font fallback,
registry reset and enable/disable selection. It also checks all three custom
text atlas layouts, exact RGBA/shadow placement, bounds, content matching and
toggle/reset behavior. Run it with the user's decompressed
USA code binary and QBF:

```sh
cmake --build build-macos --target oot3d_hd_font_check
build-macos/bin/Release/oot3d_hd_font_check /path/to/code-decompressed.bin /path/to/ltn16.qbf /path/to/sys8.qbf
```

The optional third argument also runs the native IA4 upload routine for all
17 system glyphs, checking RGBA orientation, adjacent 8-pixel cells, exact
matching, unsupported-symbol fallback, reset, and toggle behavior.

The Graphics font selector offers Original, HD Thin, and HD Heavy. It applies
immediately and persists as `Graphics/font_mode` (0, 1, 2). When that key is
absent, the old `Graphics/hd_font` boolean maps to Original or HD Heavy, keeping
existing users' appearance. The boolean is also maintained for older builds.
Both HD modes use the same outlined system date/time artwork. Tests cover all
196 main-font glyphs and custom atlases in both weights, plus live selection
between native, thin, and heavy views. No native text metrics are changed.
