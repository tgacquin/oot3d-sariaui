// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hd_rupee_font.h"
#include "oot3d/hud_routing.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc,char** argv) {
    assert(argc==2 || argc==3);
    std::ifstream input(argv[1],std::ios::binary);assert(input);
    std::vector<std::uint8_t> native((std::istreambuf_iterator<char>(input)),{});
    const auto image=OOT3D::HDFont::MakeRupeeAtlas(native);
    assert(image.size()==256*128*16*4);
    // Untouched punctuation and right-side number styles remain byte-exact
    // after upscaling. Check the vertical orientation against native Morton.
    for(unsigned y=0;y<128;++y)for(unsigned x=120;x<256;++x) {
        unsigned morton=0;
        for(unsigned b=0;b<3;++b)morton|=((x>>b)&1)<<(b*2)|((y>>b)&1)<<(b*2+1);
        const auto p=72+2*((y/8*32+x/8)*64+morton);
        const unsigned v=native[p]|unsigned(native[p+1])<<8;
        for(unsigned c=0;c<4;++c)assert(image[((511-y*4)*1024+x*4)*4+c]==((v>>(12-c*4))&15)*17);
    }
    unsigned covered=0,partial=0;
    for(unsigned y=0;y<48;++y)for(unsigned x=0;x<480;++x) {
        const auto a=image[((511-y)*1024+x)*4+3];covered+=a>0;partial+=a>0 && a<255;
    }
    assert(covered>1000 && partial>500);
    using namespace OOT3D::HDFont;
    RupeeShadow shadow;shadow.enabled=false;
    const auto plain=MakeRupeeAtlas(native,shadow);
    shadow.enabled=true;shadow.strength=0;
    assert(plain==MakeRupeeAtlas(native,shadow) && plain!=image);
    shadow={true,-1,1,1,.6f};
    const auto soft=MakeRupeeAtlas(native,shadow);
    assert(soft.size()==image.size() && soft!=image && soft!=plain);
    auto source=std::make_shared<RupeeAtlas>();source->hash=123;source->native=native;source->rgba=image;
    std::atomic_store(&rupee_atlas,std::shared_ptr<const RupeeAtlas>(source));
    assert(StyledRupeeAtlas({})==source);
    const auto styled=StyledRupeeAtlas(shadow);
    assert(styled->rgba==soft && styled->variant && StyledRupeeAtlas(shadow)==styled);
    shadow.x=0;
    assert(StyledRupeeAtlas(shadow)->variant!=styled->variant);
    assert(StyledRupeeAtlas({})==source);
    // Timer banks are 128 pixels apart, with an 8-pixel colon at each end.
    const auto timer=MakeRupeeAtlas(native,{},true);
    assert(timer.size()==image.size() && timer!=image);
    for(unsigned bank=0;bank<2;++bank)for(unsigned glyph=0;glyph<11;++glyph) {
        unsigned ink=0,aa=0;
        for(unsigned y=60*4;y<81*4;++y)
            for(unsigned x=(bank*128+glyph*12)*4;x<(bank*128+glyph*12+(glyph==10?8:12))*4;++x) {
                const auto p=((511-y)*1024+x)*4;
                if(timer[p+3] && timer[p]>128) {
                    ++ink;aa+=timer[p+3]<255;
                    if(bank)assert(timer[p]>timer[p+1] && timer[p]>timer[p+2]);
                }
            }
        assert(ink>20 && aa>10);
    }
    const auto timer_style=StyledRupeeAtlas(shadow,true);
    const auto rupee_style=StyledRupeeAtlas(shadow);
    assert(timer_style!=rupee_style && timer_style->variant!=rupee_style->variant);
    assert(StyledRupeeAtlas(shadow,true)==timer_style && StyledRupeeAtlas(shadow)==rupee_style);
    const auto ammo=StyledRupeeAtlas(shadow,false,true);
    assert(ammo->variant!=rupee_style->variant && StyledRupeeAtlas(shadow,false,true)==ammo);
    for(unsigned y=0;y<512;++y)for(unsigned x=0;x<1024;++x)for(unsigned c=0;c<4;++c) {
        const unsigned at=(y*1024+x)*4+c;
        if(y>=480 && x>=992)assert(ammo->rgba[at]==255);
        else assert(ammo->rgba[at]==rupee_style->rgba[at]);
    }
    for(bool timer_font:{false,true})for(bool backdrop:{false,true}) {
        const auto thin=StyledRupeeAtlas({},timer_font,backdrop,false);
        const auto bold=StyledRupeeAtlas({},timer_font,backdrop,true);
        assert(bold && bold->rgba.size()==thin->rgba.size() && bold->rgba!=thin->rgba);
        assert(bold->variant!=thin->variant && bold==StyledRupeeAtlas({},timer_font,backdrop,true));
        assert(StyledRupeeAtlas({},timer_font,backdrop,false)->rgba==thin->rgba);
    }
    shadow.blur=3;assert(MakeRupeeAtlas(native,shadow).empty());
    assert(OOT3D::HDFont::MakeRupeeAtlas({}).empty());
    native[48]^=1;assert(OOT3D::HDFont::MakeRupeeAtlas(native).empty());
    using namespace OOT3D::HUD;
    PublishMesh({123,3,true,456,true});
    assert(IsHDCounterMesh(123,456,3));
    assert(IsHDCounterMesh(789,456,3)); // Native color-only blink/fade.
    assert(!IsHDCounterMesh(123,456,2) && !IsHDCounterMesh(789,999,3));
    PublishMesh({234,1,true,567,true}); // Small-key counter uses the same toggle.
    assert(IsHDCounterMesh(234,567,1));
    PublishMesh({235,3,true,568,false}); // Unrelated digits stay native.
    assert(!IsHDCounterMesh(235,568,3));
    PublishMesh({345,3,false,456,true});
    assert(!IsHDCounterMesh(345,456,3) && !IsHDCounterMesh(789,456,3));
    PublishTimerMesh({987,5,false,654,false});
    assert(IsTimerMesh(987,5) && !IsTimerMesh(987,4) && !IsTimerMesh(123,3));
    assert(!IsRouted(987,5) && !IsHDCounterMesh(987,654,5));
    if(argc==3) {
        std::ofstream output(argv[2],std::ios::binary);
        output.write(reinterpret_cast<const char*>(image.data()),image.size());assert(output);
    }
    std::cout<<"HD rupees: native atlas, orientation, coverage, malformed assets, owned counter, color fades and unrelated draws passed\n";
}
