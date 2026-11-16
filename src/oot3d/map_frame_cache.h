// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/map_frame_asset.h"
#include <QByteArray>
#include <QCryptographicHash>

namespace OOT3D::MapFrame {
inline QByteArray Digest(const QByteArray& bytes) {
    return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);
}
constexpr qsizetype CacheHeaderSize=72, CacheSize=CacheHeaderSize+sizeof(Pixels);
inline QByteArray EncodeCache(const Pixels& pixels,const QByteArray& source_hash) {
    if(source_hash.size()!=32)return {};
    QByteArray payload(sizeof(Pixels),0);
    for(unsigned i=0;i<pixels.size();++i)for(unsigned c=0;c<4;++c)
        payload[4*i+c]=char(pixels[i]>>(c*8));
    QByteArray bytes("OOTFRM01",8);
    bytes.append(source_hash).append(Digest(payload)).append(payload);
    return bytes;
}
inline std::shared_ptr<Pixels> DecodeCache(const QByteArray& bytes,const QByteArray& source_hash) {
    if(source_hash.size()!=32 || bytes.size()!=CacheSize || bytes.first(8)!=QByteArray("OOTFRM01",8) ||
       bytes.mid(8,32)!=source_hash)return {};
    const auto payload=bytes.mid(CacheHeaderSize);
    if(bytes.mid(40,32)!=Digest(payload))return {};
    auto pixels=std::make_shared<Pixels>();
    const std::span<const std::uint8_t> raw{reinterpret_cast<const std::uint8_t*>(payload.constData()),std::size_t(payload.size())};
    for(unsigned i=0;i<pixels->size();++i)(*pixels)[i]=LE32(raw,i*4);
    return pixels;
}
}
