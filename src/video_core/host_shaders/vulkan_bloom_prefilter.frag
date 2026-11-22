// SPDX-License-Identifier: GPL-2.0-or-later
#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D scene;
layout(push_constant) uniform SourceRect {
    vec2 scale;
    vec2 offset;
};

void main() {
    vec2 size = vec2(textureSize(scene, 0));
    vec2 center = uv * size;
    vec2 footprint = fwidth(center);
    vec2 low = max(center - 0.5 * footprint, round(offset * size));
    vec2 high = min(center + 0.5 * footprint, round((offset + scale) * size));
    ivec2 first = ivec2(floor(low));
    ivec2 end = ivec2(ceil(high));

    // Preserve each highlight's contribution as it crosses a cell boundary.
    // Apply the brightness curve before averaging so thin bright edges survive.
    vec3 sum = vec3(0.0);
    for (int y = first.y; y < end.y; ++y) {
        float wy = min(float(y + 1), high.y) - max(float(y), low.y);
        for (int x = first.x; x < end.x; ++x) {
            float wx = min(float(x + 1), high.x) - max(float(x), low.x);
            vec3 rgb = texelFetch(scene, ivec2(x, y), 0).rgb;
            float brightness = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
            sum += rgb * smoothstep(0.62, 0.94, brightness) * (wx * wy);
        }
    }
    vec2 area = high - low;
    color = vec4(sum / (area.x * area.y), 1.0);
}
