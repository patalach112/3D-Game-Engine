#version 450
layout(location = 0) in vec3 vertexColor;
layout(location = 1) in float cameraDepth;
layout(location = 2) flat in float isGrid;
layout(location = 0) out vec4 outColor;
void main() {
    vec3 color = vertexColor;
    if (isGrid > 0.5) {
        float strength = clamp(1.0 - max(0.0, cameraDepth - 10.0) / 490.0, 0.0, 1.0);
        color = mix(vec3(39.0, 39.0, 41.0) / 255.0, color, strength);
    }
    outColor = vec4(color, 1.0);
}
