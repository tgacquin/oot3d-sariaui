// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hd_font.h"
#include "oot3d/hd_font_data.h"
#include "oot3d/hd_font_thin_data.h"
#include "oot3d/hd_system_font_data.h"
#include "oot3d/file_status_assets.h"
#include "oot3d/title_prompt_assets.h"
#include "oot3d/pause_prompt_assets.h"
#include "dynarmic/interface/A32/a32.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>

using namespace OOT3D::HDFont;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) const {
        const auto it = words.find(a);
        return it == words.end() ? std::nullopt : std::optional<U32>{it->second};
    }
};

// Execute the unmodified native A4 atlas copy routine, including its native
// Morton lookup table. The host replacement is tested against that output,
// rather than an assumed synthetic glyph layout.
struct Native : Dynarmic::A32::UserCallbacks {
    std::vector<std::uint8_t> memory = std::vector<std::uint8_t>(0x1000000);
    Dynarmic::A32::Jit* cpu{};
    U64 ticks{};
    template<class T> T Get(U32 a) {
        assert(a + sizeof(T) <= memory.size()); T v;
        std::memcpy(&v, memory.data() + a, sizeof(T)); return v;
    }
    template<class T> void Put(U32 a, T v) {
        assert(a + sizeof(T) <= memory.size()); std::memcpy(memory.data() + a, &v, sizeof(T));
    }
    std::optional<U32> MemoryReadCode(U32 a) override {
        return a == 0xf00000 ? 0xef000000 : Get<U32>(a);
    }
    std::uint8_t MemoryRead8(U32 a) override { return Get<std::uint8_t>(a); }
    std::uint16_t MemoryRead16(U32 a) override { return Get<std::uint16_t>(a); }
    U32 MemoryRead32(U32 a) override { return Get<U32>(a); }
    U64 MemoryRead64(U32 a) override { return Get<U64>(a); }
    void MemoryWrite8(U32 a, std::uint8_t v) override { Put(a,v); }
    void MemoryWrite16(U32 a, std::uint16_t v) override { Put(a,v); }
    void MemoryWrite32(U32 a, U32 v) override { Put(a,v); }
    void MemoryWrite64(U32 a, U64 v) override { Put(a,v); }
    void InterpreterFallback(U32, std::size_t) override { std::abort(); }
    void ExceptionRaised(U32, Dynarmic::A32::Exception) override { std::abort(); }
    void AddTicks(U64 n) override { ticks = n > ticks ? 0 : ticks - n; }
    U64 GetTicksRemaining() override { return ticks; }
    void CallSVC(U32 svc) override { assert(svc == 0); cpu->HaltExecution(); }
    void Copy(std::span<const std::uint8_t> glyph, unsigned w, unsigned h, unsigned x, unsigned y, bool ia4=false) {
        assert(glyph.size() == 128);
        std::copy(glyph.begin(), glyph.end(), memory.begin() + 0x810000);
        cpu->Reset(); cpu->ClearCache();
        cpu->SetCpsr(0x10);
        auto& r = cpu->Regs();
        r[0] = 0; r[1] = 0x800000; r[2] = w; r[3] = h;
        r[13] = 0xe00000; r[14] = 0xf00000; r[15] = ia4?0x2d23e0:0x2d2504;
        Put<U32>(r[13], 0x810000); Put<U32>(r[13]+4,ia4?8:16); Put<U32>(r[13]+8,16);
        Put<U32>(r[13]+12,x); Put<U32>(r[13]+16,y);
        ticks = 100000; cpu->ClearHalt(); cpu->Run(); assert(ticks);
    }
    std::vector<std::uint8_t> Decode(unsigned w, unsigned h, bool a8 = false) {
        std::vector<std::uint8_t> out(w*h);
        for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
            unsigned m = 0;
            for (unsigned b = 0; b < 3; ++b) m |= ((x>>b)&1)<<(2*b) | ((y>>b)&1)<<(2*b+1);
            const unsigned p = ((y/8)*(w/8) + x/8)*64 + m;
            out[(h-1-y)*w+x] = a8 ? memory[0x820000+p] :
                ((memory[0x800000+p/2] >> (p%2 ? 4 : 0)) & 15)*17;
        }
        return out;
    }
};

void CheckSystemArtwork(const std::vector<char>& code,const char* path) {
    std::ifstream file(path,std::ios::binary);
    const std::vector<std::uint8_t> font{std::istreambuf_iterator<char>(file),{}};
    assert(LoadSystemArtwork(font));
    Native env;std::copy(code.begin(),code.end(),env.memory.begin()+0x100000);
    Dynarmic::A32::UserConfig config;config.callbacks=&env;
    Dynarmic::A32::Jit cpu(config);env.cpu=&cpu;
    const unsigned count=font[4]|unsigned(font[5])<<8;
    auto glyph=[&](unsigned codepoint) {
        for(unsigned i=0;i<count;++i) {
            const unsigned at=16+i*8;
            if((font[at]|unsigned(font[at+1])<<8)==codepoint) {
                const unsigned index=font[at+2]|unsigned(font[at+3])<<8;
                return std::span<const std::uint8_t>(font).subspan(16+count*8+index*128,128);
            }
        }
        std::abort();
    };
    const auto decode=[&](unsigned w,unsigned h) {
        std::vector<std::uint8_t> out(w*h);
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
            unsigned morton=0;for(unsigned bit=0;bit<3;++bit)morton|=((x>>bit)&1)<<(bit*2)|((y>>bit)&1)<<(bit*2+1);
            out[(h-1-y)*w+x]=env.memory[0x800000+((y/8)*(w/8)+x/8)*64+morton];
        }
        return out;
    };
    for(const auto& record:SystemArtwork::Records) {
        std::fill_n(env.memory.begin()+0x800000,32*32,0xf0);
        env.Copy(glyph(record.code),32,32,8,8,true);
        const auto native=decode(32,32),before=native;
        const auto out=RedrawSystem(native,32,32);
        assert(out.size()==128*128*4 && native==before);
        std::vector<std::uint8_t> expected;
        for(unsigned p=record.offset;p<record.offset+record.size;p+=5)
            for(unsigned i=0;i<SystemArtwork::Runs[p];++i)
                expected.insert(expected.end(),SystemArtwork::Runs+p+1,SystemArtwork::Runs+p+5);
        assert(expected.size()==32*64*4);
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<32;++x)for(unsigned k=0;k<4;++k)
            assert(out[((32+y)*128+32+x)*4+k]==expected[((63-y)*32+x)*4+k]);
        // IA4 luminance is part of recognition, not merely glyph alpha.
        auto changed=native;changed[(8+4)*32+8]^=0x10;
        assert(RedrawSystem(changed,32,32).empty());
    }
    // Adjacent glyphs occupy one 8-pixel column, not the dialogue font's 16.
    std::fill_n(env.memory.begin()+0x800000,32*32,0xf0);
    for(unsigned i=0;i<4;++i)env.Copy(glyph('0'+i),32,32,i*8,8,true);
    auto native=decode(32,32);assert(!RedrawSystem(native,32,32).empty());
    env.Copy(glyph('#'),32,32,0,8,true);
    const auto mixed=RedrawSystem(decode(32,32),32,32);assert(!mixed.empty());
    std::fill_n(env.memory.begin()+0x800000,32*32,0xf0);
    env.Copy(glyph('#'),32,32,0,8,true);
    assert(RedrawSystem(decode(32,32),32,32).empty());
    auto bad=font;bad[13]=16;const auto generation=ArtworkGeneration();
    assert(!LoadSystemArtwork(bad));assert(ArtworkGeneration()>generation);
    assert(RedrawSystem(native,32,32).empty());
    assert(LoadSystemArtwork(font));
    Descriptor descriptor{0x14000000,32,32,9};assert(Supported(descriptor) && ByteSize(descriptor)==1024);
    Register(descriptor,42);enabled=true;assert(Find(descriptor)==42);
    enabled=false;assert(!Find(descriptor));enabled=true;
    Reset();assert(RedrawSystem(native,32,32).empty());
    std::cout<<"All 17 date/time glyphs pass native IA4 ARM uploads, RGBA orientation, adjacent cells, exact matching, reset and toggle checks\n";
}

void CheckArtwork(const std::vector<char>& code, const char* font_path, bool use_thin=false) {
    SetMode(use_thin?Thin:Heavy);
    std::ifstream file(font_path,std::ios::binary);
    const std::vector<std::uint8_t> font((std::istreambuf_iterator<char>(file)),{});
    assert(LoadArtwork(font)); assert(ArtworkGlyphCount() == 196);
    const auto original_font = font;
    Native env;
    std::copy(code.begin(),code.end(),env.memory.begin()+0x100000);
    Dynarmic::A32::UserConfig config; config.callbacks = &env;
    Dynarmic::A32::Jit cpu(config); env.cpu = &cpu;
    const unsigned count = font[4] | unsigned(font[5])<<8;
    const auto native = [&](unsigned codepoint) {
        for (unsigned i = 0; i < count; ++i) {
            const unsigned at = 16 + i*8;
            if ((font[at] | unsigned(font[at+1])<<8) == codepoint) {
                const unsigned glyph = font[at+2] | unsigned(font[at+3])<<8;
                return std::span<const std::uint8_t>(font).subspan(16+count*8+glyph*128,128);
            }
        }
        std::abort();
    };
    const auto expected = [&](unsigned codepoint) {
        const auto decode=[&](auto records,auto runs) {
            std::array<std::uint8_t,64*64> image{};
            for(const auto& record:records)if(record.code==codepoint) {
                unsigned pen=0;
                for(unsigned i=record.offset;i<record.offset+record.size;i+=2) {
                    std::fill_n(image.begin()+pen,runs[i],runs[i+1]);pen+=runs[i];
                }
                assert(pen==image.size());return image;
            }
            std::abort();
        };
        return use_thin?decode(std::span(ThinArtwork::Records),std::span(ThinArtwork::Runs)):
                        decode(std::span(Artwork::Records),std::span(Artwork::Runs));
    };
    const auto make_text=[&](auto& font,unsigned width,unsigned height,auto&& glyphs) {
        return MakeTextAtlas(font,width,height,glyphs,use_thin);
    };
    // Custom RGBA text must retain the cropped origin and its one-native-pixel
    // shadow. Compare every pixel, including transparent gutters and shadow RGB.
    const std::array text_glyphs{TextGlyph{'L',3,5}};
    const auto text = make_text(font,32,32,text_glyphs);
    assert(text.size() == 128*128*4);
    const auto hd_l = expected('L');
    const auto coverage = [&](int x, int y) -> unsigned {
        x -= 3*4; y -= 5*4;
        if (x<0 || y<0 || x>=7*4 || y>=64) return 0;
        return hd_l[y*64+5*4+x]; // L: native left=5, cropped width=7.
    };
    for (int y=0; y<128; ++y) for (int x=0; x<128; ++x) {
        const auto a=coverage(x,y),shadow=coverage(x-4,y-4)*4/5;
        const auto combined=a+shadow*(255-a)/255,white=combined?a*255/combined:0;
        const auto p=(y*128+x)*4;
        assert(text[p]==white && text[p+1]==white && text[p+2]==white && text[p+3]==combined);
    }
    assert(make_text(font,32,32,std::array{TextGlyph{'L',31,0}}).empty());
    assert(make_text(font,32,32,std::array{TextGlyph{'L',0,17}}).empty());
    assert(make_text(font,32,32,std::array{TextGlyph{0x1234,0,0}}).empty());
    assert(!RegisterTextAtlas(32,32,123,{}));
    const auto other_text=MakeTextAtlas(font,32,32,text_glyphs,!use_thin);
    assert(other_text.size()==text.size() && other_text!=text);
    assert(RegisterTextAtlas(32,32,123,use_thin?other_text:text,use_thin?text:other_text));
    const auto style_generation=ArtworkGeneration();
    assert(*FindTextAtlas(32,32,0,123)==text);
    SetMode(use_thin?Heavy:Thin);
    assert(ArtworkGeneration()!=style_generation && *FindTextAtlas(32,32,0,123)==other_text);
    SetMode(Original);assert(!FindTextAtlas(32,32,0,123));
    SetMode(use_thin?Thin:Heavy);assert(*FindTextAtlas(32,32,0,123)==text);
    assert(RegisterTextAtlas(32,32,123,text));
    enabled=false;
    assert(!TextAtlasCandidate(32,32,0)); assert(!FindTextAtlas(32,32,0,123));
    enabled=true;
    assert(TextAtlasCandidate(32,32,0)); assert(*FindTextAtlas(32,32,0,123)==text);
    assert(!FindTextAtlas(32,32,0,124)); assert(!FindTextAtlas(32,32,4,123));
    assert(!FindTextAtlas(64,32,0,123));
    const auto text_generation=ArtworkGeneration();
    assert(RegisterTextAtlas(32,32,123,text)); assert(ArtworkGeneration()!=text_generation);

    // Build all actual custom atlas layouts, so crop/row mistakes in keyboard,
    // pause/file prompts and PRESS START are caught before running the game.
    using namespace OOT3D;
    const auto status=FileStatusText::MakeAssets(font);
    const auto title=TitlePrompt::MakeAssets(font);
    assert(status && title);
    std::ifstream icon_file(std::filesystem::path(font_path).parent_path().parent_path()/"parts.ctxb",
                            std::ios::binary);
    const std::vector<std::uint8_t> icons((std::istreambuf_iterator<char>(icon_file)),{});
    const auto prompts=PausePrompts::MakeAssets(icons,font);
    assert(prompts);
    const auto original_status=status->pixels;
    const auto original_title=title->pixels;
    const auto original_prompts=prompts->text;
    std::vector<TextGlyph> placements;
    for (auto g:status->glyphs) placements.push_back({g.code,g.x+1,g.y+2});
    assert(make_text(font,FileStatusText::Width,FileStatusText::Height,placements).size()==
           FileStatusText::Width*FileStatusText::Height*64);
    placements.clear();
    for (unsigned i=0; i<TitlePrompt::Text.size(); ++i)
        placements.push_back({unsigned(TitlePrompt::Text[i]),title->glyphs[i].x,2});
    assert(make_text(font,TitlePrompt::Width,TitlePrompt::Height,placements).size()==
           TitlePrompt::Width*TitlePrompt::Height*64);
    placements.clear();
    for (unsigned row=0; row<PausePrompts::Text.size(); ++row)
        for (unsigned i=0; i<PausePrompts::Text[row].size(); ++i)
            placements.push_back({unsigned(PausePrompts::Text[row][i]),prompts->glyphs[row][i].x,row*PausePrompts::TextRowHeight+2});
    const auto prompt_text=make_text(font,PausePrompts::TextWidth,PausePrompts::TextHeight,placements);
    assert(prompt_text.size()==PausePrompts::TextWidth*PausePrompts::TextHeight*64);
    // Generation and registration both used to reject the enlarged shared
    // atlas, silently reverting every pause/map/file-select label to native.
    for(unsigned row=0;row<PausePrompts::Text.size();++row) {
        bool visible=false;
        for(unsigned y=row*PausePrompts::TextRowHeight*Scale;y<(row*PausePrompts::TextRowHeight+20)*Scale;++y)
            for(unsigned x=0;x<PausePrompts::TextWidth*Scale;++x)
                visible|=prompt_text[(y*PausePrompts::TextWidth*Scale+x)*4+3]!=0;
        assert(visible);
    }
    assert(RegisterTextAtlas(PausePrompts::TextWidth,PausePrompts::TextHeight,456,prompt_text));
    assert(TextAtlasCandidate(PausePrompts::TextWidth,PausePrompts::TextHeight,0));
    assert(*FindTextAtlas(PausePrompts::TextWidth,PausePrompts::TextHeight,0,456)==prompt_text);
    assert(!FindTextAtlas(PausePrompts::TextWidth,PausePrompts::TextHeight,0,457));
    assert(make_text(font,1024,1024,placements).empty()); // Preserve allocation budget.
    assert(!RegisterTextAtlas(1024,1024,456,{}));
    assert(status->pixels==original_status && title->pixels==original_title && prompts->text==original_prompts);
    // Every visible character, at an 8px tile offset, including accents and
    // ligatures. Assert exact rendered coverage and zero RGB (PICA alpha font).
    for (auto record : Artwork::Records) {
        std::fill_n(env.memory.begin()+0x800000,512,0);
        env.Copy(native(record.code),32,32,8,8);
        const auto alpha = env.Decode(32,32);
        const auto before = alpha;
        const auto out = Redraw(alpha,32,32);
        assert(out.size() == 128*128*4 && alpha == before);
        const auto hd = expected(record.code);
        for (unsigned y = 0; y < 128; ++y) for (unsigned x = 0; x < 128; ++x) {
            const auto p = (y*128+x)*4;
            assert(out[p] == 0 && out[p+1] == 0 && out[p+2] == 0);
            if (x>=32 && x<96 && y>=32 && y<96)
                assert(out[p+3] == hd[(95-y)*64+x-32]);
        }
    }
    // Mixed adjacent cells in native order and an unknown icon in the last cell.
    std::fill_n(env.memory.begin()+0x800000,512,0);
    env.Copy(native('L'),32,32,0,0); env.Copy(native('g'),32,32,16,0);
    env.Copy(native(0xe9),32,32,0,16);
    std::array<std::uint8_t,128> icon; icon.fill(0x77);
    env.Copy(icon,32,32,16,16);
    auto alpha = env.Decode(32,32);
    auto out = Redraw(alpha,32,32);
    assert(out.size() == 128*128*4);
    assert(out[(32*128+96)*4+3] == 119); // Unknown embedded icon is preserved.
    // Boss Rush uses the rupee weight even when the regular HD text uses
    // the other weight. The extra channel contains only outline coverage.
    for(bool bold:{false,true}) {
        const auto face=Redraw(alpha,32,32,bold);
        const auto edged=Redraw(alpha,32,32,bold,.75f);
        assert(face.size()==out.size() && edged.size()==out.size());
        bool found_rim=false;
        for(unsigned p=0;p<face.size();p+=4) {
            assert(edged[p+3]==face[p+3]+edged[p]);
            found_rim|=edged[p]!=0;
        }
        assert(found_rim && (bold==!use_thin)==(face==out));
    }
    // The A8 decoder must produce the same alpha as the native A4 upload.
    for (unsigned i=0; i<512; ++i) {
        env.memory[0x820000+2*i]=(env.memory[0x800000+i]&15)*17;
        env.memory[0x820000+2*i+1]=(env.memory[0x800000+i]>>4)*17;
    }
    assert(env.Decode(32,32,true) == alpha);
    assert(Redraw(env.Decode(32,32,true),32,32) == out);
    assert(Redraw({},0,0).empty()); assert(Redraw({},512,512).empty());
    assert(Redraw(std::vector<std::uint8_t>(1024,17),32,32).empty());
    // Changed/custom QBF glyphs must not be treated as native artwork.
    auto changed = font;
    const auto l_offset = unsigned(native('L').data() - font.data());
    changed[l_offset] ^= 1;
    assert(LoadArtwork(changed)); assert(ArtworkGlyphCount() == 195);
    std::fill_n(env.memory.begin()+0x800000,512,0);
    env.Copy(native('L'),32,32,8,8);
    assert(Redraw(env.Decode(32,32),32,32).empty());
    auto bad = font; bad[13] = 64; assert(!LoadArtwork(bad));
    assert(ArtworkGlyphCount() == 0); assert(Redraw(alpha,32,32).empty());
    bad = font; bad.resize(bad.size()-1); assert(!LoadArtwork(bad));
    assert(LoadArtwork(font));
    const auto old_generation = ArtworkGeneration();
    ClearArtwork(); assert(ArtworkGeneration() != old_generation);
    assert(!TextAtlasCandidate(32,32,0)); assert(!FindTextAtlas(32,32,0,123));
    assert(Redraw(alpha,32,32).empty());
    assert(LoadArtwork(font)); assert(font == original_font);
    // Worst-size packed atlas performance, with fully native coverage.
    std::fill_n(env.memory.begin()+0x800000,256*256/2,0);
    for (unsigned y=0; y<256; y+=16) for (unsigned x=0; x<256; x+=16)
        env.Copy(native('e'),256,256,x,y);
    alpha = env.Decode(256,256);
    const auto start = std::chrono::steady_clock::now();
    out = Redraw(alpha,256,256); assert(out.size() == 1024*1024*4);
    std::cout << (use_thin?"HD Thin: ":"HD Heavy: ") << "All 196 glyphs pass native ARM uploads; keyboard/prompt atlases, shadows and toggle pass; 256x256 atlas: "
              << std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()
              << " ms\n";
}

int main(int argc, char** argv) {
    assert(argc == 3 || argc == 4);
    for(int mode:{Original,Thin,Heavy}) {SetMode(mode);assert(CurrentMode()==mode);}
    SetMode(99);assert(CurrentMode()==Original);
    std::ifstream file(argv[1], std::ios::binary);
    const std::vector<char> code((std::istreambuf_iterator<char>(file)), {});
    assert(code.size() > 0x400000);
    Memory rom;
    for (U32 a : {HookPC, HookPC + 4, HookPC - 4, 0x2ccf30u}) {
        U32 word;
        std::memcpy(&word, code.data() + a - 0x100000, 4);
        rom.words[a] = word;
    }
    assert(CodeMatches(rom));
    rom.words[HookPC + 4] ^= 1;
    assert(!CodeMatches(rom));
    assert(Instruction(HookPC, HookWord) == (0xef000000 | FirstSVC));
    assert(Instruction(HookPC + 4, HookWord) == HookWord);
    assert(Instruction(HookPC, HookWord ^ 1) == (HookWord ^ 1));

    Memory m;
    constexpr U32 context = 0x10000, table = 0x20000, object = 0x30000, engine = 0x40000;
    m.words = {{0x54cc54,context},{0x54a814,table},{context+0x58,0},{context+0x5c,0},
        {table,object},{object+0x38,0x14000000},{object+0x40,256},{object+0x44,128},
        {object+0x50,11},{engine+0x3e0,256},{engine+0x3e4,128}};
    auto d = NativeTexture(m,engine);
    assert(d && ByteSize(*d) == 16384);
    m.words[context+0x5c] = 1;
    assert(!NativeTexture(m,engine));
    m.words[table+0x810] = 0x50000; m.words[0x50000] = object;
    assert(NativeTexture(m,engine) == d);
    m.words[engine+0x3e0] = 128;
    assert(!NativeTexture(m,engine));
    m.words[engine+0x3e0] = 256; m.words[object+0x50] = 0;
    assert(!NativeTexture(m,engine)); // RGBA UI is not a font.

    Reset(); assert(!enabled);
    Register(*d,123); assert(!Find(*d));
    enabled = true; assert(Find(*d) == 123);
    Register(*d,456); assert(Find(*d) == 456); assert(registry.size() == 1);
    auto other = *d; other.width /= 2; assert(!Find(other));
    enabled = false; assert(!Find(*d));
    enabled = true;
    for (unsigned i = 0; i < 300; ++i) { other.address = 0x20000000 + i*0x10000; Register(other,i); }
    assert(registry.size() == 256); assert(!Find(*d));
    Reset(); assert(!Find(other));

    CheckArtwork(code, argv[2]);
    CheckArtwork(code, argv[2],true);
    SetMode(Heavy);
    if(argc==4)CheckSystemArtwork(code,argv[3]);
}
