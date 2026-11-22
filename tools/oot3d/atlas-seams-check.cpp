// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/hud_hooks.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <map>
using namespace OOT3D;
using TitleScreen::U32;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
    void Write(U32 a,U32 v) {assert(words.contains(a));words[a]=v;}
    void F(U32 a,float f) {words[a]=std::bit_cast<U32>(f);}
    float F(U32 a) {return std::bit_cast<float>(words.at(a));}
};
int main(int argc,char** argv) {
    assert(argc==3); // Decompressed code, then the user's extracted ROMFS.
    const auto read=[](const std::string& path) {
        std::ifstream f(path,std::ios::binary);assert(f);
        return std::vector<unsigned char>{std::istreambuf_iterator<char>(f),{}};
    };
    const auto code=read(argv[1]);
    const auto atlas=read(std::string(argv[2])+"/menu/01_US_ENGLISH/menu_top_parts00.ctxb");
    const auto dungeon_atlas=read(std::string(argv[2])+"/menu/01_US_ENGLISH/menu_map_parts00.ctxb");
    const auto cursor_atlas=read(std::string(argv[2])+"/menu/01_US_ENGLISH/menu_cursor00.ctxb");
    const auto gear_atlas=read(std::string(argv[2])+"/menu/01_US_ENGLISH/menu_equip_parts00.ctxb");
    const auto le16=[](const auto& data,unsigned a) {return unsigned(data.at(a))|(unsigned(data.at(a+1))<<8);};
    const auto native=[&](U32 address) {
        const unsigned a=address-0x100000;
        const U32 word=le16(code,a)|(le16(code,a+2)<<16);
        return std::bit_cast<float>(word);
    };
    // 0x46ac98 passes these actual dimensions to the native helper constructor.
    // Build magic UVs from its ROM tables, not a duplicated synthetic atlas.
    const float magic_width=native(0x46b060),magic_height=native(0x46b05c);
    assert(magic_width==le16(atlas,44) && magic_height==le16(atlas,46));
    assert(magic_width==512 && magic_height==256 && le16(atlas,48)==0x6752);
    const auto texel=[&](unsigned x,unsigned y,unsigned channel) {
        unsigned morton=0;
        for(unsigned i=0;i<3;++i)morton|=((x>>i)&1)<<(2*i)|((y>>i)&1)<<(2*i+1);
        const unsigned at=72+(((y/8)*unsigned(magic_width/8)+x/8)*64+morton)*2;
        return float((le16(atlas,at)>>(12-channel*4))&15)/15;
    };
    const auto sample=[&](float u,unsigned channel) {
        const float x=u*magic_width-.5f;const unsigned left=unsigned(std::floor(x));
        const float t=x-left;
        return texel(left,123,channel)*(1-t)+texel(left+1,123,channel)*t;
    };
    constexpr U32 model=0x8000000,helper=0x8100000,mesh=0x8200000,src=0x8300000,dst=0x8400000,banner=0x8500000;
    {
        const auto actions=read(std::string(argv[2])+"/menu/01_US_ENGLISH/hud_all00.ctxb");
        assert(le16(actions,44)==256 && le16(actions,46)==256);
        // Attack occupies [0,64) x [94,112). Check begins immediately below it.
        const auto alpha=[&](unsigned x,unsigned y) {
            unsigned morton=0;
            for(unsigned i=0;i<3;++i)morton|=((x>>i)&1)<<(2*i)|((y>>i)&1)<<(2*i+1);
            return le16(actions,72+((y/8*32+x/8)*64+morton)*2)&15;
        };
        assert(alpha(13,111)==0 && alpha(13,112)>0);
        Memory m;m.words[HUD::Action+0x20]=model;m.words[HUD::Action+0x18]=helper;
        m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[helper]=8;m.words[helper+0x14]=src;
        for(unsigned i=0;i<8*32;i+=4) {m.F(src+i,0);m.F(dst+i,0);}
        for(unsigned v=0;v<4;++v) {
            m.F(src+32+v*8,float((v/2)*64)/256);
            m.F(src+36+v*8,1-(94.f+(v%2)*18)/256);
        }
        assert(AtlasSeams::Copy(m,mesh,dst,src,8*32));
        for(unsigned v=0;v<4;++v) {
            assert(m.F(dst+32+v*8)==(v/2?63.5f:.5f)/256);
            assert(m.F(dst+36+v*8)==1-(v%2?111.5f:94.5f)/256);
        }
        for(unsigned i=0;i<8*32;i+=4)if(i<32 || i>=64)assert(m.F(dst+i)==m.F(src+i));
    }
    // Every native floor slot, independent of dungeon and mirrored MQ geometry.
    for(unsigned floor=0;floor<8;++floor)for(float width:{128.f,256.f,512.f}) {
        Memory m;m.words[0x55b450+4*floor]=model;m.words[0x55b470+4*floor]=helper;
        m.words[model]=0x4ebd60;m.words[model+4]=mesh;m.words[helper]=2;m.words[helper+0x14]=src;
        m.F(helper+4,width);m.F(helper+8,256);
        for(unsigned q=0;q<2;++q)for(unsigned v=0;v<4;++v) {
            m.F(src+q*32+v*8,(q*32+(v%2)*16)/width);
            m.F(src+q*32+v*8+4,1-(32+(v/2)*24)/256.f);
            m.F(dst+q*32+v*8,0);m.F(dst+q*32+v*8+4,0);
        }
        const auto original=m.words;
        assert(!AtlasSeams::CopyDungeonRooms(m,mesh+4,dst,src,64) && m.words==original);
        assert(AtlasSeams::CopyDungeonRooms(m,mesh,dst,src,64));
        for(unsigned q=0;q<2;++q)for(unsigned v=0;v<4;++v) {
            assert(m.words.at(src+q*32+v*8)==original.at(src+q*32+v*8));
            assert(std::abs(m.F(dst+q*32+v*8)-(q*32+(v%2?15.5f:.5f))/width)<1e-6f);
            assert(std::abs(m.F(dst+q*32+v*8+4)-(1-(v/2?55.5f:32.5f)/256.f))<1e-6f);
        }
    }
    for(unsigned count:{92u,30u,3u,50u,8u,25u,78u,108u,41u}) {
        Memory m;m.words[model]=0x4ebd60;m.words[model+4]=mesh;
        m.words[helper]=count;m.words[helper+0x14]=src;
        if(count==92){m.words[HUD::Main+0xc]=model;m.words[HUD::Main+4]=helper;}
        if(count==30){m.words[0x506cb0+4]=model;m.words[0x506cb0+0x10]=helper;}
        if(count==50){m.words[0x506cb0+0xc]=model;m.words[0x506cb0+0x14]=helper;}
        if(count==41){m.words[0x50446c+4]=model;m.words[0x50446c+0xc]=helper;}
        if(count==78 || count==108) {
            const U32 owner=count==78?0x50a508:0x5093e4;
            m.words[owner+4]=model;m.words[owner+8]=helper;
        }
        if(count==3){m.words[HUD::Action+0x38]=banner;m.words[banner+0x10]=model;m.words[banner+0x14]=helper;}
        if(count==8 || count==25) {
            m.words[0x506cb0+(count==8?0x50:0x2c)]=banner;
            m.words[banner]=mesh;m.words[banner+4]=model;m.words[banner+8]=helper;m.words[banner+0xc]=0;
            assert(le16(cursor_atlas,44)==128 && le16(cursor_atlas,46)==128);
        }
        for(unsigned i=0;i<count*8;++i) {m.F(src+4*i,0);m.F(dst+4*i,99);}
        const auto uv=[&](unsigned q,float x,float y,float w,float h,float width) {
            const float height=(count==8 || count==25)?128.f:256.f;
            const std::array<float,8> values{x/width,1-y/height,(x+w)/width,1-y/height,
                x/width,1-(y+h)/height,(x+w)/width,1-(y+h)/height};
            for(unsigned i=0;i<8;++i)m.F(src+q*32+i*4,values[i]);
        };
        if(count==92)for(unsigned q=59;q<=62;++q) {
            constexpr U32 table=0x50b578;
            uv(q,native(table+0x2e0+q*8),native(table+0x2e0+q*8+4),
                native(table+q*8),native(table+q*8+4),magic_width);
        }
        if(count==3){uv(0,0,192,144,64,256);uv(1,144,192,-144,64,256);uv(2,152,176,96,9,256);}
        if(count==30){uv(12,344,168,8,40,512);uv(13,350,168,1,40,512);uv(14,352,168,-8,40,512);
            uv(15,320,64,96,32,512);uv(16,320,152,20,23,512);uv(17,320,128,24,24,512);}
        if(count==50) {
            const float width=native(0x481154),height=native(0x481150);
            assert(width==512 && height==256);
            assert(width==le16(dungeon_atlas,44) && height==le16(dungeon_atlas,46));
            for(unsigned q=0;q<50;++q)
                uv(q,native(0x507210+q*8),native(0x507214+q*8),
                    native(0x507080+q*8),native(0x507084+q*8),width);
            // 44281c updates labels at runtime, including basement floors.
            for(unsigned q=31;q<=35;++q)uv(q,64,float(q-31)*22,32,22,512);
        }
        if(count==8 || count==25)for(unsigned q=0;q<count;++q) {
            const U32 sizes=count==8?0x4d2878:0x4fa90c,coords=count==8?0x4d28b8:0x4fa9d4;
            uv(q,native(coords+q*8),native(coords+q*8+4),native(sizes+q*8),native(sizes+q*8+4),128);
        }
        if(count==78 || count==108)for(unsigned q=0;q<count;++q) {
            const U32 sizes=count==78?0x50aa2c:0x5093e4+0x728;
            const U32 coords=count==78?sizes+0x270:0x5093e4+0xa88;
            uv(q,native(coords+q*8),native(coords+q*8+4),native(sizes+q*8),native(sizes+q*8+4),count==78?256:512);
        }
        if(count==41) {
            assert(le16(gear_atlas,44)==512 && le16(gear_atlas,46)==256);
            for(unsigned q=0;q<count;++q)
                uv(q,native(0x5048fc+q*8),native(0x504900+q*8),
                    native(0x50479c+q*8),native(0x5047a0+q*8),512);
        }
        if(count==108)for(unsigned q=39;q<=46;++q)uv(q,16*(q%5),48,16,16,512);
        const auto before=m.words;
        assert(!AtlasSeams::Copy(m,mesh+4,dst,src,count*32));assert(m.words==before);
        assert(!AtlasSeams::Copy(m,mesh,src,src,count*32));assert(m.words==before);
        assert(AtlasSeams::Copy(m,mesh,dst,src,count*32));
        for(unsigned i=0;i<count*32;i+=4)assert(m.words.at(src+i)==before.at(src+i));
        const auto corrected=m.words;assert(AtlasSeams::Copy(m,mesh,dst,src,count*32));assert(m.words==corrected);
        if(count==108)for(unsigned q=39;q<=46;++q) {
            assert(m.F(dst+q*32)==(16*(q%5)+.5f)/512);
            assert(m.F(dst+q*32+8)==(16*(q%5)+15.5f)/512);
            assert(m.F(dst+q*32+4)==1-48.5f/256);
            assert(m.F(dst+q*32+20)==1-63.5f/256);
        }
        if(count==78) {
            for(unsigned q:{0u,2u,3u,5u,22u,24u,25u,27u,28u,30u})
                for(unsigned c=0;c<8;c+=2)assert(m.F(dst+q*32+c*4)<=119.5f/256);
            for(unsigned q:{1u,4u,23u,29u})for(unsigned c=0;c<8;c+=2)
                assert(m.F(dst+q*32+c*4)==118.5f/256);
        }
        if(count==108) {
            assert(m.F(dst)==56.5f/512 && m.F(dst+2*32+8)==135.5f/512);
            for(unsigned q=0;q<3;++q) {
                assert(m.F(dst+q*32+4)==1-192.5f/256);
                assert(m.F(dst+q*32+20)==.5f/256);
            }
            assert(m.F(dst+32)==103.5f/512 && m.F(dst+32+8)==103.5f/512);
        }
        if(count==92) {
            for(unsigned c=0;c<8;c+=2)assert(m.F(dst+60*32+c*4)==142.5f/magic_width);
            assert(m.F(dst+59*32+8)==143.5f/magic_width && m.F(dst+61*32)==143.5f/magic_width);
            // Before correction, filtering the cap edge blends in transparent
            // WHITE pixels. Both ends of the stretched body are black already.
            assert(sample(m.F(src+59*32+8),0)>0 && sample(m.F(src+61*32),0)>0);
            for(U32 at:{dst+59*32+8,dst+60*32,dst+60*32+8,dst+61*32}) {
                assert(sample(m.F(at),0)==0 && sample(m.F(at),3)==1);
            }
            // Routing fingerprint must match the corrected GPU upload, not the source UVs.
            constexpr U32 xyz=0x8600000,rgba=0x8700000;
            m.words[helper+0x10]=xyz;m.words[helper+0x18]=rgba;
            for(unsigned i=0;i<92*48;i+=4)m.F(xyz+i,float(i));
            for(unsigned i=0;i<92*64;i+=4)m.F(rgba+i,1);
            auto hash=HUD::HashStart;
            for(auto [base,bytes]:std::array<std::array<U32,2>,3>{{{xyz,92*48},{dst,92*32},{rgba,92*64}}})
                for(unsigned i=0;i<bytes;i+=4)hash=HUD::HashWord(hash,*m.Read(base+i));
            assert(HUD::Mesh(m,helper)->hash==hash);
        }
        if(count==3)assert(m.F(dst+8)==143.5f/256 && m.F(dst+32)==143.5f/256);
        if(count==30) {
            for(unsigned c=0;c<8;c+=2)assert(m.F(dst+13*32+c*4)==350.5f/512);
            assert(m.F(dst+12*32+8)==351.5f/512 && m.F(dst+14*32)==351.5f/512);
            assert(m.F(dst+15*32)==320.5f/512);
        }
        if(count==50) {
            assert(m.F(dst+19*32)==488.5f/512 && m.F(dst+19*32+8)==511.5f/512);
            // Sample the real ETC1A4 alpha around the key. Native edge UVs
            // blend in neighboring sprites; corrected edges stay transparent.
            const auto alpha=[&](unsigned x,unsigned y) {
                const unsigned at=72+((y/8)*64+x/8)*64+((y%8)/4*2+(x%8)/4)*16;
                std::uint64_t bits=0;
                for(unsigned i=0;i<8;++i)bits|=std::uint64_t(dungeon_atlas.at(at+i))<<(i*8);
                return float((bits>>(4*(4*(x%4)+y%4)))&15)/15;
            };
            const auto sample_alpha=[&](unsigned x,float v) {
                const float y=(1-v)*256-.5f;
                const unsigned top=unsigned(std::floor(y));
                return std::lerp(alpha(x,top),alpha(x,top+1),y-top);
            };
            // The actual floor glyph borders are transparent but the adjacent
            // parchment/stone is opaque. No neighboring alpha may leak in.
            assert(alpha(63,10)>0 && alpha(64,10)==0);
            assert(alpha(96,10)>0 && alpha(95,10)==0);
            for(unsigned q=31;q<=35;++q) {
                assert(m.F(dst+q*32)==64.5f/512);
                assert(m.F(dst+q*32+8)==95.5f/512);
                assert(m.F(dst+q*32+4)==1-((q-31)*22+.5f)/256);
                assert(m.F(dst+q*32+20)==1-((q-30)*22-.5f)/256);
            }
            assert(sample_alpha(500,m.F(src+19*32+4))>0);
            assert(sample_alpha(500,m.F(src+19*32+20))>0);
            for(unsigned x=488;x<512;++x) {
                assert(sample_alpha(x,m.F(dst+19*32+4))==0);
                assert(sample_alpha(x,m.F(dst+19*32+20))==0);
            }
        }
        if(count==41) {
            const auto alpha=[&](unsigned x,unsigned y) {
                const unsigned at=72+((y/8)*64+x/8)*64+((y%8)/4*2+(x%8)/4)*16;
                std::uint64_t bits=0;
                for(unsigned i=0;i<8;++i)bits|=std::uint64_t(gear_atlas.at(at+i))<<(i*8);
                return unsigned((bits>>(4*(4*(x%4)+y%4)))&15);
            };
            // The equipment rail's transparent top edge samples the opaque
            // green panel immediately above it. Keep it on row 184 itself.
            assert(alpha(60,183)>0 && alpha(60,184)==0);
            for(unsigned q:{12u,13u,14u})assert(m.F(dst+q*32+4)==1-184.5f/256);
            assert(m.F(dst+9*32)==256.5f/512 && m.F(dst+9*32+8)==391.5f/512);
            assert(m.F(dst+1*32)==160.5f/512 && m.F(dst+2*32)==256.5f/512);
            // Every native heart-fill state follows its current crop.
            for(unsigned state=0;state<4;++state) {
                uv(28,float(state*42),214,42,42,512);
                assert(AtlasSeams::Copy(m,mesh,dst,src,count*32));
                assert(m.F(dst+28*32)==(state*42+.5f)/512);
                assert(m.F(dst+28*32+8)==(state*42+41.5f)/512);
            }
        }
        if(count==8 || count==25) {
            for(unsigned q=0;q<count;++q)for(unsigned axis=0;axis<2;++axis) {
                float lo=1,hi=0;
                for(unsigned v=0;v<4;++v) {const float f=m.F(src+q*32+(v*2+axis)*4);lo=std::min(lo,f);hi=std::max(hi,f);}
                for(unsigned v=0;v<4;++v) {
                    const float f=m.F(dst+q*32+(v*2+axis)*4);
                    assert(f>=lo+.5f/128 && f<=hi-.5f/128);
                }
            }
            // The actual marker sprite's transparent left edge borders opaque
            // frame pixels. Bilinear filtering previously leaked that frame.
            const auto alpha=[&](unsigned x,unsigned y) {
                unsigned morton=0;
                for(unsigned i=0;i<3;++i)morton|=((x>>i)&1)<<(2*i)|((y>>i)&1)<<(2*i+1);
                return le16(cursor_atlas,72+(((y/8)*16+x/8)*64+morton)*2)&15;
            };
            assert(alpha(15,32)>0 && alpha(16,32)==0);
            // The long top strip's inner edge samples the corner immediately
            // below it unless its V endpoint is kept on row 39's center.
            assert(alpha(10,39)==0 && alpha(10,40)>0);
            if(count==8) {
                assert(m.F(dst+32)==10.5f/128 && m.F(dst+32+8)==10.5f/128);
                assert(m.F(dst+32+20)==1-39.5f/128);
            }
            // First marker starts at column 16: its right/bottom adjacent
            // sprites are excluded as well, while the glow sprite is retained.
            if(count==25)assert(m.F(dst)==16.5f/128 && m.F(dst+8)==31.5f/128);
            m.words[banner+0xc]=1;const auto alternate=m.words;
            assert(!AtlasSeams::Copy(m,mesh,dst,src,count*32) && m.words==alternate);
            m.words[banner+0xc]=0;
        }
        // No unrelated quad changes, and vertical meter/parchment UVs stay native.
        for(unsigned q=0;q<count;++q)for(unsigned c=0;c<8;++c) {
            const bool changed=count==92?(q>=59 && q<=61 && c%2==0):
                count==41?AtlasSeams::GearDecoration(q):
                count==3?(q<2 && c%2==0):count==50?(q==19 || (q>=21 && q<=35)):(count==8 || count==25)?true:
                count==78?(q<6 || (q>=22 && q<=30) || (q>=43 && q<64)):count==108?(q<=2 || (q>=35 && q<=37) || (q>=39 && q<=46)):(q>=12 && q<=17);
            if(!changed)assert(m.words.at(dst+q*32+c*4)==m.words.at(src+q*32+c*4));
        }
        m.words[helper]=count+1;const auto invalid=m.words;
        assert(!AtlasSeams::Copy(m,mesh,dst,src,count*32));assert(m.words==invalid);
    }
    // Dungeon selection cursor: four yellow corners plus four glow corners,
    // from the native 2f2448 mode-0 tables. Reject other cursor modes/owners.
    {
        Memory m;m.words[0x506cb0+0x1c]=banner;
        m.words[banner]=mesh;m.words[banner+4]=model;m.words[banner+8]=helper;
        m.words[banner+0xc]=8;m.words[banner+0x24]=0;
        m.words[model]=0x4ebd60;m.words[model+4]=mesh;
        m.words[helper]=8;m.words[helper+0x14]=src;
        for(unsigned q=0;q<8;++q)for(unsigned v=0;v<4;++v) {
            const float x=native(0x4d2590+q*8),y=native(0x4d2594+q*8);
            m.F(src+q*32+v*8,(x+(v%2)*16)/128);
            m.F(src+q*32+v*8+4,1-(y+(v/2)*16)/128);
            m.F(dst+q*32+v*8,0);m.F(dst+q*32+v*8+4,0);
        }
        const auto original=m.words;
        assert(AtlasSeams::Copy(m,mesh,dst,src,8*32));
        for(unsigned q=0;q<8;++q)for(unsigned v=0;v<4;++v) {
            const float x=native(0x4d2590+q*8),y=native(0x4d2594+q*8);
            assert(m.F(dst+q*32+v*8)==(x+(v%2?15.5f:.5f))/128);
            assert(m.F(dst+q*32+v*8+4)==1-(y+(v/2?15.5f:.5f))/128);
            assert(m.words.at(src+q*32+v*8)==original.at(src+q*32+v*8));
        }
        for(unsigned invalid:{1u,2u}) {
            m.words[banner+0x24]=invalid;const auto before=m.words;
            assert(!AtlasSeams::Copy(m,mesh,dst,src,8*32) && m.words==before);
        }
    }
    std::cout<<"Atlas seams: dungeon room crops (all eight floors), gear heart/Triforce/item rails, magic body/caps, map plate/name/icons, blue frame/markers, dungeon key/floor labels, yellow cursor/glow corners, parchment join, ownership, fresh uploads and routing hash passed\n";
}
