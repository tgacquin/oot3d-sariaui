// SPDX-License-Identifier: GPL-2.0-or-later
#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D highlights;
// Shared with the fullscreen triangle vertex shader.
layout(push_constant) uniform SourceRect {
    vec2 scale;
    vec2 offset;
};

void main() {
    vec2 half_texel = 0.5 / vec2(textureSize(highlights, 0));
    vec2 inset = min(half_texel, abs(scale) * 0.5);
    vec2 low = min(offset, offset + scale) + inset;
    vec2 high = max(offset, offset + scale) - inset;
    // This kernel runs only on the small glow texture. Screen derivatives keep
    // its radius consistent across native, widescreen and interpolated images.
    vec2 dx = dFdx(uv) * 0.65;
    vec2 dy = dFdy(uv) * 0.65;
    const float weights[7] = float[](1., 6., 15., 20., 15., 6., 1.);
    vec3 glow = vec3(0);
    for (int y = -3; y <= 3; ++y) {
        for (int x = -3; x <= 3; ++x) {
            vec2 p = clamp(uv + float(x) * dx + float(y) * dy, low, high);
            glow += textureLod(highlights, p, 0.).rgb * weights[x+3] * weights[y+3];
        }
    }
    color = vec4(glow / 4096., 1.);
}
