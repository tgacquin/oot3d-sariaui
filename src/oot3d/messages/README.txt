Controller message translations

Catalogs are keyed by the game's stable message IDs. English (QM slot 1), French
(slot 5) and Spanish (slot 7) cover the same 78 controller messages.

Find/replace strings use UTF-8 and hexadecimal QM controls, for example
{24:06} for the A icon and {1d:43} for a color. Newlines become native line breaks.
Script controls, including choices, timing, variables and page boundaries,
must survive edits. The compiler checks this and embeds only changed spans,
with a fingerprint of each original message. It needs the user's extracted
archive to regenerate; the runtime uses their cartridge and writes no ROM files.

From the repository root:
python3 tools/oot3d/build-message-patches.py /path/to/us.qm src/oot3d/messages/english.json src/oot3d/messages/english_patches.h

Generate french_patches.h and spanish_patches.h using the same command with
their respective JSON catalogs. The runtime installs all three catalogs as one
verified transaction, so changing language selects the right native slot.
message-patch-check.cpp and message-catalog-check.py verify locale isolation,
control flow, font widths and the absence of old Navi/eye touch instructions.

Tutorials 1017 and 103F use the implemented NaviUp binding in all three languages.
10C7 uses the implemented SongListStart binding. Start enters the native song
list from playable ocarina mode; B returns to playing before cancelling. All
three languages use these bindings. Input regression: tools/oot3d/ocarina-check.cpp.

Explicit user RomFS replacements/patches take precedence. Unsupported message
archives are left untouched and logged. Baked menu-atlas captions are separate
assets and are not rewritten by this message catalog.

Difficulty price changes use a separate price_patches.h catalog. When enabled,
price digits in 324 purchase/fee messages are updated in English, French and
Spanish together. Other words, quantities, rewards, mask-loan repayments and
message commands remain unchanged. Both original and control-edited English
payloads are recognized. No verified price text means no increased charges.
The price factor is latched at application startup; changing it needs a restart.
Regenerate using:
python3 tools/oot3d/build-price-patches.py /path/to/us.qm src/oot3d/messages/price_patches.h
