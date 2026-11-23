// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace OOT3D::HDFont {
// Runtime reads native signatures from the user's ROM; we only ship our new art.
bool LoadArtwork(std::span<const std::uint8_t> qbf);
bool LoadSystemArtwork(std::span<const std::uint8_t> qbf);
void ClearSystemArtwork();
std::uint64_t SystemArtworkGeneration();
// Bottom-up decoded IA4 bytes; includes the system font's baked dark outline.
std::vector<std::uint8_t> RedrawSystem(std::span<const std::uint8_t> ia4,
                                     unsigned width, unsigned height);
void ClearArtwork();
std::uint64_t ArtworkGeneration();
unsigned ArtworkGlyphCount();

// Custom UI uses cropped glyphs in RGBA atlases with baked drop shadows.
// Positions remain in native pixels; the host image alone becomes 4x larger.
struct TextGlyph { unsigned code, x, y; };
std::vector<std::uint8_t> MakeTextAtlas(std::span<const std::uint8_t> qbf,
    unsigned width, unsigned height, std::span<const TextGlyph> glyphs, bool thin=false);
bool RegisterTextAtlas(unsigned width, unsigned height, std::uint64_t native_hash,
                       std::vector<std::uint8_t> rgba, std::vector<std::uint8_t> thin_rgba={});
bool TextAtlasCandidate(unsigned width, unsigned height, unsigned format);
std::shared_ptr<const std::vector<std::uint8_t>> FindTextAtlas(
    unsigned width, unsigned height, unsigned format, std::uint64_t native_hash);

// Input/output follow Azahar's bottom-up decoded texture orientation. Only
// byte-exact native glyph cells are replaced. Unknown fonts/packs stay native.
// Empty means no matching glyphs: use the existing native texture view.
std::vector<std::uint8_t> Redraw(std::span<const std::uint8_t> alpha,
                               unsigned width, unsigned height, std::optional<bool> bold={}, float outline=0);
} // namespace OOT3D::HDFont
