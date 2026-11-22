// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/surface_capture.h"
#include <cmath>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace OOT3D::Interpolation::SurfaceCapture {
namespace {
template<class Values> QJsonArray ValuesJSON(const Values& values) {
    QJsonArray result;
    for (const auto value : values)
        result.append(std::isfinite(double(value)) ? QJsonValue(double(value)) : QJsonValue(QJsonValue::Null));
    return result;
}
template<class Rows> QJsonArray RowsJSON(const Rows& rows) {
    QJsonArray result;
    for (const auto& row : rows) result.append(ValuesJSON(row));
    return result;
}
QJsonObject DrawJSON(const Draw& d) {
    QJsonArray textures, outputs, inputs, effect_inputs;
    for (const auto& t : d.textures) textures.append(QJsonObject{
        {"enabled",double(t.enabled)},{"address",double(t.address)},
        {"width",double(t.width)},{"height",double(t.height)},{"format",double(t.format)},
        {"wrap_s",double(t.wrap_s)},{"wrap_t",double(t.wrap_t)},
        {"min_filter",double(t.min_filter)},{"mag_filter",double(t.mag_filter)},{"type",double(t.type)}});
    for (unsigned i = 0; i < d.output_count && i < d.output_vertices.size(); ++i) {
        const auto& v = d.output_vertices[i];
        outputs.append(QJsonObject{{"clip_position",ValuesJSON(v.position)},
            {"color",ValuesJSON(v.color)},{"uv012",ValuesJSON(v.uv)},{"uv0_w",v.uv0_w}});
    }
    for (unsigned i = 0; i < d.input_count && i < d.inputs.size(); ++i)
        inputs.append(RowsJSON(d.inputs[i]));
    for(const auto& input:d.effect_inputs)effect_inputs.append(RowsJSON(input));
    QJsonObject draw{{"frame",double(d.frame)},{"time_us",double(d.time_us)},{"elapsed_ms",double(d.elapsed_ms)},
        {"ordinal",double(d.ordinal)},{"generation",double(d.generation)},{"scene",double(d.scene)},
        {"shader",QString::number(d.shader,16)},{"swizzle",QString::number(d.swizzle,16)},
        {"target",double(d.target)},{"vertices",double(d.vertices)},{"vertex_base",double(d.vertex_base)},
        {"index_offset",double(d.index_offset)},{"first_vertex",double(d.first_vertex)},
        {"last_vertex",double(d.last_vertex)},{"topology",double(d.topology)},
        {"textbox_active",d.textbox_active},{"textbox_match",d.textbox_match},
        {"textbox_hash",QString::number(d.textbox_hash,16)},{"textbox_source",ValuesJSON(d.textbox_source)},
        {"cpu",d.cpu},{"indexed",d.indexed},{"index16",d.index16},{"succeeded",d.succeeded},
        {"uniforms_f",RowsJSON(d.uniforms)},{"uniforms_i",RowsJSON(d.integer_uniforms)},
        {"uniforms_b_mask",double(d.boolean_uniforms)},{"textures",textures},{"tev",RowsJSON(d.tev)},
        {"blend",ValuesJSON(d.blend)},{"blend_color",double(d.blend_color)},
        {"attribute_registers",ValuesJSON(d.attribute_registers)},
        {"alpha_test",ValuesJSON(d.alpha_test)},{"depth",ValuesJSON(d.depth)},
        {"output_vertices",outputs},{"input_attributes",inputs},{"attribute_count",int(d.attribute_count)},
        {"effect_inputs",effect_inputs},{"effect_order",ValuesJSON(d.effect_order)}};
    if (d.magic_fill) {
        const auto& m=*d.magic_fill;
        draw["magic_fill"]=QJsonObject{{"left",ValuesJSON(m.left)},{"right",ValuesJSON(m.right)},
            {"replay_start",ValuesJSON(m.replay_start)},{"alpha",m.alpha},{"moving",m.moving}};
    }
    return draw;
}
}
bool Export(const Packet& data, const std::string& path) {
    const auto filename = QString::fromStdString(path);
    if (!QDir().mkpath(QFileInfo(filename).absolutePath())) return false;
    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QJsonArray textboxes;
    for(const auto& t:data.textboxes)textboxes.append(QJsonObject{
        {"frame",double(t.frame)},{"elapsed_ms",double(t.elapsed_ms)},{"generation",double(t.generation)},
        {"model",double(t.model)},{"mesh",double(t.mesh)},{"pane",double(t.pane)},
        {"state",double(t.state)},{"flags",double(t.flags)},{"vertices",double(t.vertices)},
        {"buffer",double(t.buffer)},{"hash",QString::number(t.hash,16)},
        {"matrix",RowsJSON(t.matrix)},{"stage",QString::fromStdString(t.stage)}});
    const QJsonObject header{{"format","oot3d-surface-capture-v1"},{"session",int(data.session)},
        {"delay_ms",3000},{"duration_ms",10000},{"sample_frames","frame % 10 < 3"},
        {"max_per_frame",int(MaxPerFrame)},{"max_per_250ms",int(MaxPerBucket)},
        {"frame_retention","First 128 and last 64 draws if over budget; use ordinal to order retained draws."},
        {"textbox_samples",textboxes},{"textbox_dropped",int(data.textbox_dropped)},
        {"dropped_draws",double(data.dropped)},{"truncated",data.dropped != 0},
        {"notes","Native top-screen color draws before midpoint replay; texture metadata only, no pixel readback. Geometry addresses, shader and ordinal are matching hints, not guaranteed persistent effect IDs. CPU vertices are clip-space; accelerated vertices contain input attributes. TEV rows: sources, modifiers, ops, RGBA, scales. Blend: enable, srcRGB, dstRGB, srcAlpha, dstAlpha, eqRGB, eqAlpha."}};
    auto bytes = QJsonDocument(header).toJson(QJsonDocument::Compact);
    bytes.chop(1); bytes += ",\"draws\":[";
    if (file.write(bytes) != bytes.size()) return false;
    // Stream each draw instead of constructing a second full capture in memory.
    bool first = true;
    for (const auto& draw : data.draws) {
        bytes = QJsonDocument(DrawJSON(draw)).toJson(QJsonDocument::Compact);
        if (!first) bytes.prepend(',');
        first = false;
        if (file.write(bytes) != bytes.size()) return false;
    }
    return file.write("]}",2) == 2 && file.commit();
}
}
