// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hd_font_artwork.h"
#include "oot3d/hd_font.h"
#include "oot3d/hd_font_data.h"
#include "oot3d/hd_font_thin_data.h"
#include <memory>
#include <unordered_map>

namespace OOT3D::HDFont {
namespace {
constexpr unsigned Cell = 16, HDCell = Cell * Scale;
struct Glyph {
    unsigned code{}, left{}, width{};
    std::array<std::uint8_t, Cell * Cell> native;
    std::array<std::uint8_t, HDCell * HDCell> hd;
    std::array<std::uint8_t, HDCell * HDCell> thin_hd;
};
struct Face {
    std::vector<Glyph> glyphs;
    std::unordered_multimap<U64, unsigned> by_hash;
};
std::shared_ptr<const Face> artwork;
std::atomic<U64> generation{0};
struct TextAtlas {
    unsigned width, height;
    U64 hash;
    std::shared_ptr<const std::vector<std::uint8_t>> rgba;
    std::shared_ptr<const std::vector<std::uint8_t>> thin_rgba;
};
std::mutex text_mutex;
std::vector<TextAtlas> text_atlases;

U64 Hash(std::span<const std::uint8_t> data) {
    U64 hash = 14695981039346656037ULL;
    for (auto b : data) hash = (hash ^ b) * 1099511628211ULL;
    return hash;
}
unsigned LE16(std::span<const std::uint8_t> b, unsigned i) {
    return unsigned(b[i]) | (unsigned(b[i + 1]) << 8);
}
bool TextSizeSupported(unsigned width, unsigned height) {
    // Prompt labels now occupy a 128x1024 atlas. Permit tall/wide atlases
    // while retaining the previous 512x512 total-pixel allocation limit.
    return width >= 8 && height >= 8 && width <= 1024 && height <= 1024 &&
           width * height <= 512 * 512;
}
} // namespace

void ClearArtwork() {
    ClearSystemArtwork();
    { std::lock_guard lock(text_mutex); text_atlases.clear(); }
    std::atomic_store(&artwork, std::shared_ptr<const Face>{});
    ++generation;
}
U64 ArtworkGeneration() { return (generation.load()+SystemArtworkGeneration())*2+thin.load(); }
unsigned ArtworkGlyphCount() {
    const auto face = std::atomic_load(&artwork);
    return face ? unsigned(face->glyphs.size()) : 0;
}
bool LoadArtwork(std::span<const std::uint8_t> qbf) {
    ClearArtwork();
    if (qbf.size() < 16 || qbf[0] != 'Q' || qbf[1] != 'B' || qbf[2] != 'F' ||
        qbf[3] != '1' || qbf[12] != 4 || qbf[13] != Cell || qbf[14] != Cell)
        return false;
    const unsigned count = LE16(qbf, 4), sheets = LE16(qbf, 6);
    const unsigned bitmap = 16 + count * 8;
    if (!count || count > 4096 || !sheets || sheets > 4096 ||
        qbf.size() != bitmap + sheets * 128) return false;
    auto face = std::make_shared<Face>();
    for (unsigned i = 0; i < count; ++i) {
        const unsigned record = 16 + i * 8, code = LE16(qbf, record);
        const unsigned index = LE16(qbf, record + 2);
        const unsigned left = qbf[record + 4], width = qbf[record + 5];
        if (index >= sheets || left + width > Cell) return false;
        const auto found = std::find_if(std::begin(Artwork::Records), std::end(Artwork::Records),
            [&](const auto& r) { return r.code == code && r.left == left && r.width == width; });
        if (found == std::end(Artwork::Records)) continue;
        Glyph g{};
        g.code = code; g.left = left; g.width = width;
        for (unsigned p = 0; p < Cell * Cell; ++p)
            g.native[p] = ((qbf[bitmap + index * 128 + p / 2] >> (p % 2 ? 0 : 4)) & 15) * 17;
        if (Hash(g.native) != found->native_hash) continue; // Respect modified QBF fonts.
        if (std::all_of(g.native.begin(), g.native.end(), [](auto a) { return a == 0; })) continue;
        // Generated coverage is run-length encoded (count,value), bounded here
        // as well as by the generation check, before publishing an immutable face.
        if (found->offset + found->size > std::size(Artwork::Runs) || found->size % 2) return false;
        unsigned pixel = 0;
        for (unsigned p = found->offset; p < found->offset + found->size; p += 2) {
            const unsigned length = Artwork::Runs[p];
            if (!length || pixel + length > g.hd.size()) return false;
            std::fill_n(g.hd.begin() + pixel, length, Artwork::Runs[p + 1]);
            pixel += length;
        }
        if (pixel != g.hd.size()) return false;
        const auto slim=std::find_if(std::begin(ThinArtwork::Records),std::end(ThinArtwork::Records),
            [&](const auto& r) {return r.code==code && r.left==left && r.width==width && r.native_hash==found->native_hash;});
        if(slim==std::end(ThinArtwork::Records) || slim->offset+slim->size>std::size(ThinArtwork::Runs) || slim->size%2)return false;
        pixel=0;
        for(unsigned p=slim->offset;p<slim->offset+slim->size;p+=2) {
            const unsigned length=ThinArtwork::Runs[p];
            if(!length || pixel+length>g.thin_hd.size())return false;
            std::fill_n(g.thin_hd.begin()+pixel,length,ThinArtwork::Runs[p+1]);pixel+=length;
        }
        if(pixel!=g.thin_hd.size())return false;
        face->by_hash.emplace(Hash(g.native), unsigned(face->glyphs.size()));
        face->glyphs.push_back(std::move(g));
    }
    if (face->glyphs.empty()) return false;
    std::atomic_store(&artwork, std::shared_ptr<const Face>(std::move(face)));
    ++generation;
    return true;
}

bool RegisterTextAtlas(unsigned width, unsigned height, U64 native_hash,
                       std::vector<std::uint8_t> rgba, std::vector<std::uint8_t> thin_rgba) {
    if (!TextSizeSupported(width, height) ||
        (width & (width - 1)) || (height & (height - 1)) ||
        rgba.size() != width * height * Scale * Scale * 4 ||
        (!thin_rgba.empty() && thin_rgba.size()!=rgba.size())) return false;
    std::lock_guard lock(text_mutex);
    std::erase_if(text_atlases, [&](const auto& a) {
        return a.width == width && a.height == height && a.hash == native_hash;
    });
    if (text_atlases.size() >= 16) return false;
    auto heavy=std::make_shared<const std::vector<std::uint8_t>>(std::move(rgba));
    auto light=thin_rgba.empty()?heavy:
        std::make_shared<const std::vector<std::uint8_t>>(std::move(thin_rgba));
    text_atlases.push_back({width, height, native_hash,std::move(heavy),std::move(light)});
    ++generation;
    return true;
}
bool TextAtlasCandidate(unsigned width, unsigned height, unsigned format) {
    if (!enabled.load() || format != 0) return false;
    std::lock_guard lock(text_mutex);
    return std::any_of(text_atlases.begin(), text_atlases.end(), [&](const auto& a) {
        return a.width == width && a.height == height;
    });
}
std::shared_ptr<const std::vector<std::uint8_t>> FindTextAtlas(
    unsigned width, unsigned height, unsigned format, U64 hash) {
    if (!enabled.load() || format != 0) return {};
    std::lock_guard lock(text_mutex);
    for (const auto& a : text_atlases)
        if (a.width == width && a.height == height && a.hash == hash) return thin.load()?a.thin_rgba:a.rgba;
    return {};
}

std::vector<std::uint8_t> MakeTextAtlas(std::span<const std::uint8_t> qbf,
    unsigned width, unsigned height, std::span<const TextGlyph> glyphs, bool use_thin) {
    const auto face = std::atomic_load(&artwork);
    if (!face || !TextSizeSupported(width, height) ||
        width % 8 || height % 8 || glyphs.empty() || glyphs.size() > 4096 ||
        qbf.size() < 16 || qbf[0] != 'Q' || qbf[1] != 'B' || qbf[2] != 'F' ||
        qbf[3] != '1' || qbf[12] != 4 || qbf[13] != Cell || qbf[14] != Cell) return {};
    const unsigned count = LE16(qbf, 4), sheets = LE16(qbf, 6), bitmap = 16 + count * 8;
    if (!count || count > 4096 || !sheets || sheets > 4096 ||
        qbf.size() != bitmap + sheets * 128) return {};
    const unsigned wide = width * Scale, high = height * Scale;
    std::vector<std::uint8_t> alpha(wide * high);
    unsigned replaced = 0;
    for (const auto& place : glyphs) {
        unsigned at = 16;
        while (at < bitmap && LE16(qbf, at) != place.code) at += 8;
        if (at == bitmap) return {};
        const unsigned index = LE16(qbf, at + 2), left = qbf[at + 4], crop = qbf[at + 5];
        if (index >= sheets || left + crop > Cell || place.x > width || crop > width - place.x ||
            place.y > height || Cell > height - place.y) return {};
        const auto found = std::find_if(face->glyphs.begin(), face->glyphs.end(),
            [&](const auto& g) { return g.code == place.code && g.left == left && g.width == crop; });
        std::array<std::uint8_t, Cell * Cell> native;
        for (unsigned p = 0; p < native.size(); ++p)
            native[p] = ((qbf[bitmap + index * 128 + p / 2] >> (p % 2 ? 0 : 4)) & 15) * 17;
        const bool hd = found != face->glyphs.end() && found->native == native;
        replaced += hd;
        for (unsigned y = 0; y < HDCell; ++y) for (unsigned x = 0; x < crop * Scale; ++x) {
            std::uint8_t a;
            if (hd) a = (use_thin?found->thin_hd:found->hd)[y * HDCell + left * Scale + x];
            else {
                // Preserve spaces and unsupported/modified glyphs, including
                // their original crop boundaries, within an otherwise HD atlas.
                const float sx = (x + .5f) / Scale - .5f, sy = (y + .5f) / Scale - .5f;
                const int ix = int(std::floor(sx)), iy = int(std::floor(sy));
                const auto sample = [&](int px, int py) -> float {
                    if (px < 0 || px >= int(crop) || py < 0 || py >= int(Cell)) return 0;
                    return native[py * Cell + left + px];
                };
                a = std::uint8_t(std::lerp(std::lerp(sample(ix,iy), sample(ix+1,iy), sx-ix),
                    std::lerp(sample(ix,iy+1), sample(ix+1,iy+1), sx-ix), sy-iy) + .5f);
            }
            auto& dest = alpha[(place.y * Scale + y) * wide + place.x * Scale + x];
            dest = std::max(dest, a);
        }
    }
    if (!replaced) return {};
    std::vector<std::uint8_t> out(wide * high * 4);
    for (unsigned y = 0; y < high; ++y) for (unsigned x = 0; x < wide; ++x) {
        const unsigned a = alpha[y * wide + x];
        const unsigned shadow = x >= Scale && y >= Scale ?
            alpha[(y - Scale) * wide + x - Scale] * 4 / 5 : 0;
        const unsigned combined = a + shadow * (255 - a) / 255;
        const auto white = std::uint8_t(combined ? a * 255 / combined : 0);
        // Native custom atlases flip before tiling, and Vulkan flips at decode:
        // host RGBA is therefore top-down, unlike native QBF alpha atlases.
        const unsigned p = (y * wide + x) * 4;
        out[p] = out[p+1] = out[p+2] = white;
        out[p+3] = std::uint8_t(combined);
    }
    return out;
}

std::vector<std::uint8_t> Redraw(std::span<const std::uint8_t> alpha,
                               unsigned width, unsigned height) {
    const auto face = std::atomic_load(&artwork);
    if (!face || width < Cell || height < Cell || width > 512 || height > 512 ||
        width % 8 || height % 8 || alpha.size() != width * height) return {};
    struct Match { unsigned x, y, glyph; };
    std::vector<Match> matches;
    std::array<std::uint8_t, Cell * Cell> cell;
    // The native allocator uses 8x8 blocks, and ltn16 occupies 2x2 blocks.
    // Native function 0x2d2504 copies full glyph cells, preserving their gutters.
    // The Vulkan decoder flips the atlas vertically, so undo that per cell for
    // recognition and flip the replacement back when writing its host texture.
    for (unsigned y = 0; y + Cell <= height; y += 8) {
        for (unsigned x = 0; x + Cell <= width; x += 8) {
            for (unsigned row = 0; row < Cell; ++row)
                std::copy_n(alpha.begin() + (y + Cell - 1 - row) * width + x,
                            Cell, cell.begin() + row * Cell);
            const auto [begin, end] = face->by_hash.equal_range(Hash(cell));
            for (auto it = begin; it != end; ++it) {
                // Hash alone must never allow UI textures or changed glyphs to match.
                if (face->glyphs[it->second].native == cell) {
                    matches.push_back({x, y, it->second});
                    break;
                }
            }
        }
    }
    if (matches.empty()) return {};
    // Unrecognized cells (e.g. embedded symbols) retain normal bilinear native
    // coverage. Do not silently run the old sharpening option on them.
    const unsigned wide = width * Scale, high = height * Scale;
    std::vector<std::uint8_t> out(wide * high * 4);
    std::vector<bool> covered(width / 8 * (height / 8));
    for (const auto& match : matches)
        for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 2; ++x)
            covered[(match.y / 8 + y) * (width / 8) + match.x / 8 + x] = true;
    const auto sample = [&](int x, int y) -> float {
        return alpha[std::clamp(y, 0, int(height) - 1) * width +
                     std::clamp(x, 0, int(width) - 1)];
    };
    // Avoid filtering the entire atlas before overwriting it with new glyphs.
    // Only unknown nonempty blocks need filtering; blank gutters are already 0.
    for (unsigned by = 0; by < height / 8; ++by) for (unsigned bx = 0; bx < width / 8; ++bx) {
        if (covered[by * (width / 8) + bx]) continue;
        bool ink = false;
        for (int y = -1; y <= 8 && !ink; ++y) for (int x = -1; x <= 8; ++x)
            if (sample(int(bx * 8) + x, int(by * 8) + y)) { ink = true; break; }
        if (!ink) continue;
        for (unsigned y = by * 8 * Scale; y < (by + 1) * 8 * Scale; ++y) {
            const float sy = (y + .5f) / Scale - .5f;
            const int iy = int(std::floor(sy));
            for (unsigned x = bx * 8 * Scale; x < (bx + 1) * 8 * Scale; ++x) {
                const float sx = (x + .5f) / Scale - .5f;
                const int ix = int(std::floor(sx));
                const float top = std::lerp(sample(ix, iy), sample(ix + 1, iy), sx - ix);
                const float bottom = std::lerp(sample(ix, iy + 1), sample(ix + 1, iy + 1), sx - ix);
                out[(y * wide + x) * 4 + 3] = std::uint8_t(std::lerp(top, bottom, sy - iy) + .5f);
            }
        }
    }
    const bool use_thin=thin.load();
    for (const auto& match : matches) {
        const auto& glyph=face->glyphs[match.glyph];
        const auto& hd = use_thin?glyph.thin_hd:glyph.hd;
        for (unsigned y = 0; y < HDCell; ++y)
            for (unsigned x = 0; x < HDCell; ++x)
                out[((match.y * Scale + y) * wide + match.x * Scale + x) * 4 + 3] =
                    hd[(HDCell - 1 - y) * HDCell + x];
    }
    redrawn_glyphs += unsigned(matches.size());
    return out;
}
} // namespace OOT3D::HDFont
