// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/surface_capture.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
using namespace OOT3D::Interpolation::SurfaceCapture;
int main() {
    unsigned calls=0;
    const auto fill=[&](Draw& d) {
        ++calls;d.shader=0xfedcba9876543210ULL;
        d.uniforms[30][2]=NAN;d.uniforms[31][0]=.125f;
        d.textures[0].address=0x12345678;d.textures[0].wrap_s=2;
        d.tev[0][3]=0x7fffffff;d.cpu=true;d.output_count=1;
        d.output_vertices[0].color[3]=.75f;d.output_vertices[0].uv[0]=.25f;
        d.output_vertices[0].position[0]=2.f;
        d.input_count=1;d.inputs[0][4][0]=.5f;
    };
    Observe(10,fill,4000);assert(calls==0);
    assert(Start(1000) && !Start(1000));
    assert(!Recording(3999) && Recording(4000) && Recording(13999) && !Recording(14000));
    Observe(10,fill,3999);assert(calls==0);
    for(unsigned frame=10;frame<20;++frame)Observe(frame,fill,4000);
    assert(calls==3 && draw_count==3);
    Observe(20,fill,13999);
    Observe(21,fill,14000);assert(calls==4);
    assert(!Finish(13999));auto data=Finish(14000);
    assert(data && !active && !Finish(14000) && data->dropped==0);
    assert(data->draws.back().elapsed_ms==9999);
    TextboxSample sample;sample.frame=10;sample.model=0x12340000;sample.hash=0xfedcba9876543210ULL;
    sample.stage="published";sample.matrix[0][3]=123.f;
    data->textboxes.push_back(sample);data->textbox_dropped=2;
    QTemporaryDir dir;assert(dir.isValid());const auto path=dir.path()+"/nested/surfaces.json";
    assert(Export(*data,path.toStdString()));QFile file(path);assert(file.open(QIODevice::ReadOnly));
    QJsonParseError error;
    const auto root=QJsonDocument::fromJson(file.readAll(),&error).object();
    assert(error.error==QJsonParseError::NoError && root["format"]=="oot3d-surface-capture-v1");
    const auto textbox=root["textbox_samples"].toArray()[0].toObject();
    assert(textbox["stage"]=="published" && textbox["hash"]=="fedcba9876543210");
    assert(textbox["matrix"].toArray()[0].toArray()[3]==123.f && root["textbox_dropped"]==2);
    const auto draws=root["draws"].toArray();assert(draws.size()==4);
    const auto draw=draws[0].toObject();
    assert(draw["shader"]=="fedcba9876543210");
    assert(draw["uniforms_f"].toArray()[30].toArray()[2].isNull());
    assert(draw["uniforms_f"].toArray()[31].toArray()[0]==.125);
    assert(draw["textures"].toArray()[0].toObject()["address"]==0x12345678);
    assert(draw["tev"].toArray()[0].toArray()[3]==0x7fffffff);
    const auto v=draw["output_vertices"].toArray()[0].toObject();
    assert(v["color"].toArray()[3]==.75 && v["uv012"].toArray()[0]==.25);
    assert(draw["input_attributes"].toArray()[0].toArray()[4].toArray()[0]==.5);
    assert(!Export(*data,path.toStdString()+"/bad.json"));
    assert(Start(20000));assert(draw_count==0);
    // Dense early frames must not prevent later wall-time buckets being recorded.
    calls=0;
    for(unsigned bucket=0;bucket<BucketCount;++bucket) {
        for(unsigned f=0;f<5;++f)
            for(unsigned i=0;i<MaxPerFrame+1;++i)
                Observe(f*10,fill,23000+bucket*BucketMs);
    }
    assert(calls>=MaxDraws && draw_count==MaxDraws);
    auto bounded=Finish(33000);
    assert(bounded && bounded->draws.size()==MaxDraws && bounded->dropped>0);
    assert(bounded->draws.back().elapsed_ms==9750);
    assert(bounded->draws[1].ordinal==1);
    assert(bounded->draws[MaxPerFrame-TailDraws].ordinal==MaxPerFrame);
    // Effects drawn far beyond the head budget must still be present, in
    // consecutive sampled frames, with their original ordinals.
    assert(Start(34000));
    for(unsigned frame=10;frame<13;++frame)
        for(unsigned i=0;i<1000;++i)Observe(frame,fill,37000);
    auto tail=Finish(47000);assert(tail && tail->draws.size()==3*MaxPerFrame);
    for(unsigned f=0;f<3;++f) {
        unsigned last=0;
        for(unsigned i=0;i<MaxPerFrame;++i)
            last=std::max(last,tail->draws[f*MaxPerFrame+i].ordinal);
        assert(last==999);
    }
    assert(Start(40000));Observe(10,fill,43000);
    auto reset=Finish(53000);assert(reset && reset->draws.size()==1 && reset->draws[0].ordinal==0);
    std::cout<<"Surface capture: countdown, native-frame sampling, all time buckets, caps, reset, colors/UVs/uniforms, JSON and export failure passed\n";
}
