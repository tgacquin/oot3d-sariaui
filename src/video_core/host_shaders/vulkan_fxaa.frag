// SPDX-License-Identifier: GPL-2.0-or-later
#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D scene;
layout(push_constant) uniform SourceRect {
    vec2 scale;
    vec2 offset;
};

float luma(vec3 rgb) {
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

vec3 sampleScene(vec2 position, vec2 texel) {
    // The guest framebuffer can occupy a subrectangle of a larger image.
    return textureLod(scene, clamp(position, offset + 0.5 * texel,
                                   offset + scale - 0.5 * texel), 0.0).rgb;
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(scene, 0));
    vec3 center = sampleScene(uv, texel);
    float nw = luma(sampleScene(uv - texel, texel));
    float ne = luma(sampleScene(uv + vec2(texel.x, -texel.y), texel));
    float sw = luma(sampleScene(uv + vec2(-texel.x, texel.y), texel));
    float se = luma(sampleScene(uv + texel, texel));
    float mid = luma(center);
    float low = min(mid, min(min(nw, ne), min(sw, se)));
    float high = max(mid, max(max(nw, ne), max(sw, se)));
    if (high - low < max(1.0 / 24.0, high / 8.0)) {
        color = vec4(center, 1.0);
        return;
    }

    // Filter along the local edge. Limit the span to preserve narrow details.
    vec2 direction = vec2(sw + se - nw - ne, nw + sw - ne - se);
    float bias = max((nw + ne + sw + se) / 32.0, 1.0 / 128.0);
    direction /= min(abs(direction.x), abs(direction.y)) + bias;
    vec2 span = clamp(direction, vec2(-8.0), vec2(8.0)) * texel;
    vec3 inner = 0.5 * (sampleScene(uv - span / 6.0, texel) + sampleScene(uv + span / 6.0, texel));
    vec3 wide = 0.5 * inner + 0.25 * (sampleScene(uv - span * 0.5, texel) + sampleScene(uv + span * 0.5, texel));
    float brightness = luma(wide);
    color = vec4(brightness < low || brightness > high ? inner : wide, 1.0);
}
