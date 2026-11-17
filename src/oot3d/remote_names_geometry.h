// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/remote_names.h"
#include "oot3d/hd_font_thin_data.h"
namespace OOT3D::RemoteNames {
struct Glyph {unsigned code{},x{},y{},width{};};
struct Font {
    unsigned width=512,height=0;
    std::vector<Glyph> glyphs;
    std::vector<std::uint8_t> pixels;
    const Glyph* Find(unsigned code) const {
        for(const auto& g:glyphs)if(g.code==code)return &g;
        if(code!='?')return Find('?');
        return nullptr;
    }
};
inline Font MakeFont() {
    Font f;unsigned x=1,y=1;
    for(const auto& record:HDFont::ThinArtwork::Records) {
        const unsigned width=record.width*4;
        if(x+width+1>f.width){x=1;y+=66;}
        f.glyphs.push_back({record.code,x,y,width});x+=width+2;
    }
    f.height=std::bit_ceil(y+69); // Reserve four rows for a solid backdrop sample.
    if(f.height>2048)return {};
    f.pixels.resize(f.width*f.height*4);
    for(unsigned i=0;i<f.glyphs.size();++i) {
        const auto& record=HDFont::ThinArtwork::Records[i];const auto& g=f.glyphs[i];
        std::array<std::uint8_t,4096> alpha{};unsigned n=0;
        if(record.offset+record.size>std::size(HDFont::ThinArtwork::Runs) || record.size%2 || record.left+record.width>16)return {};
        for(unsigned p=record.offset;p<record.offset+record.size;p+=2) {
            const auto len=HDFont::ThinArtwork::Runs[p];
            if(!len || n+len>alpha.size())return {};
            std::fill_n(alpha.begin()+n,len,HDFont::ThinArtwork::Runs[p+1]);n+=len;
        }
        if(n!=alpha.size())return {};
        for(unsigned row=0;row<64;++row)for(unsigned col=0;col<g.width;++col) {
            const auto at=4*((g.y+row)*f.width+g.x+col);
            f.pixels[at]=f.pixels[at+1]=f.pixels[at+2]=255;
            f.pixels[at+3]=alpha[row*64+record.left*4+col];
        }
    }
    for(unsigned row=f.height-4;row<f.height;++row)
        for(unsigned col=f.width-4;col<f.width;++col)
            for(unsigned c=0;c<4;++c)f.pixels[4*(row*f.width+col)+c]=255;
    return f;
}
// Bounded UTF-8 decoding: unsupported names use '?' rather than interpreting
// UTF-8 bytes as separate letters. Native accented file names remain supported.
inline std::vector<unsigned> Characters(const std::string& text) {
    std::vector<unsigned> out;
    for(unsigned i=0;i<text.size() && out.size()<64;) {
        unsigned c=static_cast<unsigned char>(text[i++]),n=0;
        if(c>=0xc2 && c<=0xdf){c&=31;n=1;}
        else if(c>=0xe0 && c<=0xef){c&=15;n=2;}
        else if(c>=0xf0 && c<=0xf4){c&=7;n=3;}
        else if(c>=128){out.push_back('?');continue;}
        bool valid=i+n<=text.size();
        for(unsigned j=0;j<n && valid;++j) {
            const unsigned next=static_cast<unsigned char>(text[i]);
            if((next&0xc0)!=0x80){valid=false;break;}
            c=(c<<6)|(next&63);++i;
        }
        out.push_back(valid && c>=32?c:'?');
    }
    return out;
}
struct Scene {
    CameraSample camera;
    Matrix projection{},inverse_view{};
    float clip_x=1,clip_y=1,depth_scale=-1,depth_offset=0;
    bool wbuffer=false;
};
struct Vertex {std::array<float,4> position;std::array<float,2> uv;std::array<float,4> color;};
inline std::vector<Vertex> Geometry(const Font& font,const WorldLabels& labels,const Scene& scene,const Layout& layout) {
    std::vector<Vertex> vertices;
    if(!layout.enabled || layout.opacity<=0 || !font.height)return vertices;
    const auto view=Math::InverseRigid(scene.inverse_view);
    const auto clip=Math::Multiply(scene.projection,view);
    for(const auto& label:labels.anchors) {
        std::array<double,4> center{};
        for(unsigned r=0;r<4;++r){center[r]=clip[r][3];for(unsigned c=0;c<3;++c)center[r]+=clip[r][c]*label.position[c];}
        if(!std::isfinite(center[3]) || center[3]<=1 || center[2]>0 || center[2]<-center[3])continue;
        // Landscape screen X follows clip Y. The native projection may mirror
        // that axis (regular/MQ); choose the billboard direction after projection
        // and clip scaling, including the interpolated camera and perspective.
        double horizontal_y=0,horizontal_w=0;
        for(unsigned c=0;c<3;++c) {
            horizontal_y+=clip[1][c]*labels.billboard[c][0];
            horizontal_w+=clip[3][c]*labels.billboard[c][0];
        }
        const double horizontal=(horizontal_y*center[3]-center[1]*horizontal_w)*scene.clip_y;
        const double right=horizontal<0?-1:1;
        double distance=0;
        for(unsigned c=0;c<3;++c)distance+=std::pow(label.position[c]-labels.billboard[c][3],2);
        const float fade=std::clamp(float((1200-std::sqrt(distance))/300),0.f,1.f)*layout.opacity;
        if(fade<=0)continue;
        const auto chars=Characters(label.name);
        float width=0;for(unsigned c:chars)if(c==32)width+=4;else if(const auto* g=font.Find(c))width+=g->width*.25f+1;
        const float unit=.7f*layout.scale;
        const auto point=[&](double x,double y,std::array<float,2> uv,std::array<float,4> color) {
            std::array<double,3> world=label.position;
            for(unsigned r=0;r<3;++r)
                world[r]+=(right*labels.billboard[r][0]*x+labels.billboard[r][1]*y)*unit;
            std::array<double,4> p{};
            for(unsigned r=0;r<4;++r){p[r]=clip[r][3];for(unsigned j=0;j<3;++j)p[r]+=clip[r][j]*world[j];}
            return Vertex{{float(p[0])*scene.clip_x,float(p[1])*scene.clip_y,-float(p[2]),float(p[3])},uv,color};
        };
        // Soft oval behind the complete name, independent of glyph shadows.
        // Same world plane and depth test as the text; no opaque panel edges.
        if(layout.backdrop && layout.backdrop_opacity>0 && width>0) {
            const float cx=(layout.x+layout.backdrop_x)/3.f;
            const float cy=8-(layout.y+layout.backdrop_y)/3.f;
            const float rx=width*.5f*layout.backdrop_width,ry=8*layout.backdrop_height;
            const std::array<float,2> uv{float(font.width-2)/font.width,float(font.height-2)/font.height};
            const auto oval=[&](float radius,unsigned segment) {
                const float angle=segment*6.28318530718f/32;
                const float a=layout.backdrop_softness>0?
                    std::clamp((1-radius)/layout.backdrop_softness,0.f,1.f):1;
                return point(cx+std::cos(angle)*rx*radius,cy+std::sin(angle)*ry*radius,uv,
                    {0,0,0,fade*layout.backdrop_opacity*a*a*(3-2*a)});
            };
            for(unsigned ring=0;ring<8;++ring)for(unsigned segment=0;segment<32;++segment) {
                const auto a=oval(ring/8.f,segment),b=oval((ring+1)/8.f,segment),
                    c=oval((ring+1)/8.f,segment+1),d=oval(ring/8.f,segment+1);
                vertices.insert(vertices.end(),{a,b,c,a,c,d});
            }
        }
        const auto draw=[&](float dx,float dy,float shade,float alpha) {
            float cursor=-width*.5f+layout.x/3.f+dx;
            for(unsigned c:chars) {
                if(c==32){cursor+=4;continue;}
                const auto* g=font.Find(c);if(!g)continue;
                std::array<Vertex,4> quad;
                for(unsigned k=0;k<4;++k) {
                    // Camera-facing world geometry: offsets and glyphs shrink
                    // together with distance instead of floating in screen space.
                    const double x=cursor+(k&1)*g->width*.25f;
                    const double y=16-(k>>1)*16-layout.y/3.f-dy;
                    quad[k]=point(x,y,{float(g->x+(k&1)*g->width)/font.width,float(g->y+(k>>1)*64)/font.height},
                        {shade,shade,shade,alpha});
                }
                for(unsigned index:{0u,2u,1u,1u,2u,3u})vertices.push_back(quad[index]);
                cursor+=g->width*.25f+1;
            }
        };
        if(layout.shadow && layout.shadow_strength>0) {
            const float alpha=fade*layout.shadow_strength;
            const float dx=layout.shadow_x/3,dy=layout.shadow_y/3,r=layout.shadow_softness/3;
            if(r<=.01f)draw(dx,dy,0,alpha);
            else {
                constexpr std::array<std::array<float,2>,9> offsets{{{0,0},{1,0},{-1,0},{0,1},{0,-1},{.7071f,.7071f},{-.7071f,.7071f},{.7071f,-.7071f},{-.7071f,-.7071f}}};
                for(unsigned i=0;i<offsets.size();++i)
                    draw(dx+offsets[i][0]*r,dy+offsets[i][1]*r,0,1-std::pow(1-alpha,i?.075f:.4f));
            }
        }
        draw(0,0,1,fade);
    }
    return vertices;
}
}
