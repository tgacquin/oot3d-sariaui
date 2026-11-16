// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <optional>
#include <span>
#include <vector>
#include <cstdint>

namespace OOT3D::Interpolation {
// OoT submits non-indexed strips: degenerate connector vertices have different
// input indices even when their attributes are identical. Canonicalize only
// those connectors for the shared decoders, then update every duplicate in the
// replay. Leaving a connector at its native position creates a stray triangle.
template<class V,class Apply> std::optional<std::vector<V>> ReplayEffectStrip(
    std::span<const V> inputs,std::span<const std::uint32_t> order,Apply apply) {
    if(inputs.empty() || inputs.size()>768 || order.empty() || order.size()>768 || order.size()%6)return {};
    for(auto i:order)if(i>=inputs.size())return {};
    std::vector<V> canonical;
    std::vector<std::uint32_t> indices;
    std::vector<int> aliases(inputs.size(),-1);
    const auto bind=[&](unsigned original,unsigned target) {
        if(aliases[original]>=0 && aliases[original]!=int(target))return false;
        aliases[original]=int(target);return true;
    };
    for(unsigned i=0;i<order.size();i+=6) {
        const unsigned base=canonical.size();
        for(unsigned k=0;k<4;++k) {
            if(!bind(order[i+k],base+k))return {};
            canonical.push_back(inputs[order[i+k]]);indices.push_back(base+k);
        }
        if(inputs[order[i+4]]!=inputs[order[i+3]] || !bind(order[i+4],base+3))return {};
        indices.push_back(base+3);
        indices.push_back(base+4);
        if(i+6<order.size()) {
            if(inputs[order[i+5]]!=inputs[order[i+6]] || !bind(order[i+5],base+4))return {};
        } else {
            if(!bind(order[i+5],base+4))return {};
            canonical.push_back(inputs[order[i+5]]);
        }
    }
    const auto half=apply(canonical,indices);
    if(!half || half->size()!=canonical.size())return {};
    std::vector<V> result(inputs.begin(),inputs.end());
    for(unsigned i=0;i<inputs.size();++i)if(aliases[i]>=0)result[i]=(*half)[aliases[i]];
    return result;
}
}
