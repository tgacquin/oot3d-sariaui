// SPDX-License-Identifier: GPL-2.0-or-later
#define main ExistingSurfaceCheck
#include "surface-effects-check.cpp"
#undef main
int main(int argc,char** argv) {
    assert(argc==2);
    QFile file(argv[1]);assert(file.open(QIODevice::ReadOnly));
    const auto json=QJsonDocument::fromJson(file.readAll()).object();
    assert(!json["truncated"].toBool() && json["dropped_draws"].toInt()==0);
    for(float phase:{.25f,.5f,.75f,1.f}) {
        OOT3D::Interpolation::sample_fraction=phase;
        E::History history;
        struct Previous {qint64 frame{};std::vector<E::Quad> quads;};
        std::map<unsigned,Previous> previous;
        unsigned batches=0,nodes=0,tips=0;
        for(const auto value:json["draws"].toArray()) {
            const auto d=value.toObject();const auto regs=Regs(d);
            if(E::Candidate(regs,d["shader"].toString().toULongLong(nullptr,16),
                d["swizzle"].toString().toULongLong(nullptr,16))!=1)continue;
            const auto b=CapturedEffect(d);const auto current=E::Forest(b.inputs,b.order);assert(current);
            assert(E::ForestTrail(*current,0) && E::ForestTrail(*current,8));++batches;
            history.Context(d["generation"].toInt(),d["scene"].toInt());
            const auto frame=d["frame"].toInteger();
            auto& old=previous[E::F::Kind(current->front().vertices[0])];
            const auto half=OOT3D::Interpolation::ReplayEffectStrip<E::Vertex>(b.inputs,b.order,
                [&](const auto& v,const auto& indices){return history.Apply(1,frame,
                    regs.texturing.texture0.GetPhysicalAddress(),v,indices);});
            if(old.frame+1==frame) {
                assert(half);const auto out=E::Forest(*half,b.order);assert(out);
                for(unsigned i=0;i<16;++i) {
                    const auto a=E::F::Center(old.quads[i]),z=E::F::Center((*current)[i]),h=E::F::Center((*out)[i]);
                    for(unsigned c=0;c<3;++c)assert(std::abs(h[c]-std::lerp(a[c],z[c],phase))<.0001f);
                    assert(std::abs(D::Size((*out)[i])-std::lerp(D::Size(old.quads[i]),D::Size((*current)[i]),phase))<.0001f);
                    if(i%8==0 && E::F::Kind((*current)[i].vertices[0])==1) {
                        // Actual tip rotation is twenty degrees per update.
                        const auto edge=[](const E::Quad& q){std::array<float,3> v;for(unsigned c=0;c<3;++c)v[c]=q.vertices[1][0][c]-q.vertices[0][0][c];return v;};
                        const auto before=edge(old.quads[i]),after=edge((*out)[i]);
                        float dot=0,aa=0,bb=0;for(unsigned c=0;c<3;++c){dot+=before[c]*after[c];aa+=before[c]*before[c];bb+=after[c]*after[c];}
                        const float angle=std::acos(std::clamp(dot/std::sqrt(aa*bb),-1.f,1.f));
                        assert(std::abs(angle-phase*20.f*3.14159265f/180)<.001f);++tips;
                    }
                    ++nodes;
                }
                assert(half->back()==b.inputs.back());
                if(phase==1)for(unsigned i=0;i<16;++i)for(unsigned k=0;k<4;++k)for(unsigned c=0;c<3;++c)
                    assert(std::abs((*out)[i].vertices[k][0][c]-(*current)[i].vertices[k][0][c])<.0001f);
                auto cut=*current;for(unsigned i=0;i<8;++i)for(auto& v:cut[i].vertices)v[0][0]+=100;
                assert(!E::ForestTrailPair(cut,old.quads,0));
                auto reordered=*current;std::swap(reordered[2],reordered[3]);assert(!E::ForestTrail(reordered,0));
                reordered=*current;for(unsigned i=0;i<8;++i)std::swap(reordered[i],reordered[i+8]);
                assert(!E::ForestTrailPair(reordered,old.quads,0));
            } else assert(!half);
            old={frame,*current};
        }
        std::cout<<"Kokiri trails phase="<<phase<<" batches="<<batches<<" nodes="<<nodes<<" tips="<<tips<<'\n';
        assert(batches==178 && nodes==1888 && tips==118);
    }
}
