#version 450
layout(location=0) in vec4 position;
layout(location=1) in vec4 packedNormal;
struct ObjectData { vec3 position; int rotation; float scale; uint texturePair; float radius; uint padding; };
layout(std430,set=0,binding=0) readonly buffer Objects { ObjectData objects[]; };
layout(std430,set=0,binding=2) readonly buffer Tints { vec4 tints[]; };
layout(push_constant) uniform Camera { vec4 right; vec4 up; vec4 forward; vec4 eye; vec4 projection; } camera;
layout(location=0) out vec3 vertexColor;
layout(location=1) out float cameraDepth;
layout(location=2) flat out float isGrid;
vec3 rotate_xyz(vec3 p,uint rotation) {
    vec3 r=vec3(rotation&1023u,(rotation>>10)&1023u,(rotation>>20)&1023u)*(6.283185307179586/1024.0);
    vec3 c=cos(r),s=sin(r);
    p=vec3(p.x,p.y*c.x-p.z*s.x,p.y*s.x+p.z*c.x);
    p=vec3(p.x*c.y+p.z*s.y,p.y,-p.x*s.y+p.z*c.y);
    return vec3(p.x*c.z-p.y*s.z,p.x*s.z+p.y*c.z,p.z);
}
void main() {
    ObjectData object=objects[gl_InstanceIndex];
    vec3 world=rotate_xyz(position.xyz*object.scale,uint(object.rotation))+object.position;
    vec3 delta=world-camera.eye.xyz;
    float x=dot(delta,camera.right.xyz),y=dot(delta,camera.up.xyz),z=dot(delta,camera.forward.xyz);
    float n=camera.projection.z,f=camera.projection.w;
    gl_Position=vec4(x*camera.projection.x,-y*camera.projection.y,f/(f-n)*z-f*n/(f-n),z);
    vec3 normal=normalize(rotate_xyz(packedNormal.xyz*2.0-1.0,uint(object.rotation)));
    vertexColor=tints[gl_InstanceIndex].rgb*(0.48+0.52*abs(dot(normal,normalize(vec3(-0.4,0.8,0.55)))));
    cameraDepth=z; isGrid=0;
}
