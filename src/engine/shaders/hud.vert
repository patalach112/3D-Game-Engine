#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec4 color;
layout(push_constant) uniform Screen { vec4 size; } screen;
layout(location=0) out vec4 vertexColor;
void main() { gl_Position=vec4(position/screen.size.xy*2.0-1.0,0,1); vertexColor=color; }
