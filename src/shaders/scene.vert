#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in float grid;
layout(push_constant) uniform Camera {
    vec4 right;
    vec4 up;
    vec4 forward;
    vec4 eye;
    vec4 projection; // x/y focal scale, near, far; Vulkan depth in [0, 1]
} camera;
layout(location = 0) out vec3 vertexColor;
layout(location = 1) out float cameraDepth;
layout(location = 2) flat out float isGrid;
void main() {
    vec3 delta = position - camera.eye.xyz;
    float x = dot(delta, camera.right.xyz);
    float y = dot(delta, camera.up.xyz);
    float z = dot(delta, camera.forward.xyz);
    float n = camera.projection.z, f = camera.projection.w;
    gl_Position = vec4(x * camera.projection.x, -y * camera.projection.y,
                       f / (f - n) * z - f * n / (f - n), z);
    float lighting = 1.0;
    if (dot(normal, normal) > 0.01)
        lighting = 0.48 + 0.52 * abs(dot(normalize(normal), normalize(vec3(-0.4, 0.8, 0.55))));
    vertexColor = color * lighting;
    cameraDepth = z;
    isGrid = grid;
}
