#version 450
layout(location=0) in vec4 position;
layout(location=1) in vec3 color;
layout(location=3) in float grid;
layout(push_constant) uniform Camera { vec4 right; vec4 up; vec4 forward; vec4 eye; vec4 projection; vec4 debugScale; } camera;
layout(location=0) out vec3 vertexColor;
layout(location=1) out float cameraDepth;
layout(location=2) flat out float isGrid;
void main() {
    vec3 delta=position.xyz*camera.debugScale.x;
    float x=dot(delta,camera.right.xyz),y=dot(delta,camera.up.xyz),z=dot(delta,camera.forward.xyz);
    float n=camera.projection.z,f=camera.projection.w;
    gl_Position=vec4(x*camera.projection.x,-y*camera.projection.y,f/(f-n)*z-f*n/(f-n),z);
    vertexColor=color; cameraDepth=z; isGrid=grid;
}
