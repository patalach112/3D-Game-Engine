// Projection, grid, selection outlines and mode-specific gizmo geometry.
#include "engine/render/internal_types.hpp"
#include "engine/gizmo.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace engine {
using namespace internal;
ScreenPoint project_to_screen(Vec3 world,const EditorCamera& camera,int width,int height) {
    if(width<=0||height<=0) return {};
    const Basis basis=camera_basis(camera);
    const CameraPoint p=to_camera(world,camera,basis);
    if(p.z<kNearPlane) return {};
    const float focal=(static_cast<float>(height)*0.5F)/std::tan(camera.vertical_fov_degrees*kPi/360.0F);
    return {static_cast<float>(width)*0.5F+p.x*focal/p.z,
            static_cast<float>(height)*0.5F-p.y*focal/p.z,p.z,true};
}

Ray3 screen_ray(float x,float y,const EditorCamera& camera,int width,int height) {
    const Basis basis=camera_basis(camera);
    const float tangent=std::tan(camera.vertical_fov_degrees*kPi/360.0F);
    const float nx=(2.0F*x/static_cast<float>(std::max(1,width))-1.0F)
                  *(static_cast<float>(width)/static_cast<float>(std::max(1,height)))*tangent;
    const float ny=(1.0F-2.0F*y/static_cast<float>(std::max(1,height)))*tangent;
    return {camera.position,normalize(add(basis.forward,add(multiply(basis.right,nx),multiply(basis.up,ny))))};
}
namespace internal {
Vec3 rgb(std::uint32_t value) {
    return {static_cast<float>((value >> 16U) & 255U) / 255.0F,
            static_cast<float>((value >> 8U) & 255U) / 255.0F,
            static_cast<float>(value & 255U) / 255.0F};
}
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

FrameGeometry scene_geometry(const Scene& scene, const EditorCamera& camera, EntityId selected,
                             int width, int height, bool grid_visible, int gizmo_mode) {
    std::vector<DebugVertex> lines, overlay_lines, overlay_triangles;
    const auto line = [](std::vector<DebugVertex>& out, Vec3 a, Vec3 b, Vec3 color, float grid = 0.0F) {
        out.push_back({a, color, grid}); out.push_back({b, color, grid});
    };
    for (const auto& entity : scene.entities()) {
        if (entity.primitive!=PrimitiveType::Cube && entity.id!=selected) continue;
        const auto& mesh = primitive_mesh(entity.primitive);
        const auto& t = entity.transform;
        if (!finite(t.position) || !finite(t.rotation) || !finite(t.scale)) continue;
        const auto rotation=unpack_rotation(pack_rotation(t.rotation));
        const float rx=rotation.x*kPi/180.0F, ry=rotation.y*kPi/180.0F, rz=rotation.z*kPi/180.0F;
        const float cx=std::cos(rx),sx=std::sin(rx),cy=std::cos(ry),sy=std::sin(ry),cz=std::cos(rz),sz=std::sin(rz);
        std::vector<Vec3> world;
        world.reserve(mesh.vertices.size());
        for (auto p : mesh.vertices) {
            p={p.x*t.scale.x,p.y*t.scale.y,p.z*t.scale.z};
            const Vec3 px{p.x,p.y*cx-p.z*sx,p.y*sx+p.z*cx};
            const Vec3 py{px.x*cy+px.z*sy,px.y,-px.x*sy+px.z*cy};
            world.push_back(add({py.x*cz-py.y*sz,py.x*sz+py.y*cz,py.z}, t.position));
        }
        if (entity.primitive == PrimitiveType::Cube || entity.id == selected) {
            for (const auto& edge : mesh.edges)
                line(lines, world[edge[0]], world[edge[1]],
                     rgb(entity.id == selected ? 0x00F1A06BU : 0x00555558U));
        }
    }

    GeometryBuilder builder;
    std::array<MeshId,6> cached{};
    std::array<bool,6> has_mesh{};
    for (const auto& entity:scene.entities()) {
        const auto& t=entity.transform;
        if (!finite(t.position)||!finite(t.rotation)||!finite(t.scale)) continue;
        const bool uniform=t.scale.x==t.scale.y&&t.scale.y==t.scale.z;
        const auto type=static_cast<std::size_t>(entity.primitive);
        const auto& mesh=primitive_mesh(entity.primitive);
        MeshId id{};
        if (uniform&&has_mesh[type]) id=cached[type];
        else {
            std::vector<engine::Vertex> vertices;
            std::vector<std::uint32_t> indices;
            const auto local=[&](Vec3 p) { return uniform?p:Vec3{p.x*t.scale.x,p.y*t.scale.y,p.z*t.scale.z}; };
            for (const auto& triangle:mesh.triangles) {
                const auto a=local(mesh.vertices[triangle[0]]),b=local(mesh.vertices[triangle[1]]),c=local(mesh.vertices[triangle[2]]);
                const auto normal=normalize(cross(subtract(b,a),subtract(c,a)));
                for (auto p:{a,b,c}) { indices.push_back(static_cast<std::uint32_t>(vertices.size())); vertices.push_back(make_vertex(p,normal)); }
            }
            id=builder.add_mesh(vertices,indices);
            if (uniform) { cached[type]=id; has_mesh[type]=true; }
        }
        ObjectData object;
        object.position=t.position; object.rotation=pack_rotation(t.rotation); object.scale=uniform?t.scale.x:1;
        const auto base=entity.id==selected?0x00D8D8DAU:entity.primitive==PrimitiveType::Sphere?0x007FA8D8U:
            entity.primitive==PrimitiveType::Cylinder?0x007DAF82U:entity.primitive==PrimitiveType::Plane?0x00B2A27DU:0x00A7A7AAU;
        const auto tint=rgb(base); builder.add_object(id,object,{tint.x,tint.y,tint.z,1});
    }
    // Ground grid bounds follow the frustum, rather than a fixed little patch.
    const Basis basis=camera_basis(camera);
    const float v_tan=std::tan(camera.vertical_fov_degrees*kPi/360.0F);
    if (grid_visible) {
        const float h_tan=v_tan*static_cast<float>(width)/static_cast<float>(height);
        float gx0=camera.position.x-10, gx1=camera.position.x+10;
        float gz0=camera.position.z-10, gz1=camera.position.z+10;
        const float horizon=std::max(50.0F,std::abs(camera.position.y)*100.0F);
        for (float x : {-1.0F,1.0F}) for (float y : {-1.0F,1.0F}) {
            const Vec3 dir=add(basis.forward,add(multiply(basis.right,x*h_tan),multiply(basis.up,y*v_tan)));
            const float hit_t=std::abs(dir.y)>0.0001F?-camera.position.y/dir.y:-1;
            const float distance=hit_t>0?std::min(hit_t,horizon):horizon;
            const Vec3 hit=add(camera.position,multiply(dir,distance));
            gx0=std::min(gx0,hit.x); gx1=std::max(gx1,hit.x);
            gz0=std::min(gz0,hit.z); gz1=std::max(gz1,hit.z);
        }
        const float range=std::max(gx1-gx0,gz1-gz0);
        // Nice-steps: wybierz krok z serii 1-2-5-10-20-50... aby linie miały rozmiar ~1/10 range.
        const float raw_step=range/12.0F;
        float magnitude=1.0F;
        while (magnitude*2.0F<=raw_step) magnitude*=2.0F;
        float step=magnitude;
        while (step*5.0F<=raw_step) step*=5.0F;
        step=std::max(step,1.0F);
        // Bounded iteration also protects against hostile/very large scene coordinates.
        for (int axis=0;axis<2;++axis) {
            const float start=std::floor((axis?gz0:gx0)/step)*step;
            const float end=axis?gz1:gx1;
            for (int i=0;i<2048;++i) {
                const float c=start+static_cast<float>(i)*step;
                if (c>end) break;
                const bool major=std::abs(std::remainder(c,5.0F*step))<0.01F;
                const Vec3 color=rgb(major?0x008A8A90U:0x005A5A5EU);
                if (axis) line(lines,{gx0-20,0,c},{gx1+20,0,c},color,1);
                else line(lines,{c,0,gz0-20},{c,0,gz1+20},color,1);
            }
        }
    }
    if (const Entity* entity=scene.find_entity(selected)) {
        const Vec3 center=entity->transform.position;
        const float handle_length=gizmo_length(camera.position,center);
        const float pixel_radius=static_cast<float>(std::clamp(static_cast<int>(std::lround(height*0.011F)),7,12));
        if (gizmo_mode==1) {
            for (int axis=0;axis<3;++axis) for (int i=0;i<64;++i)
                line(overlay_lines,gizmo_ring_point(center,axis,handle_length,2*kPi*i/64),
                    gizmo_ring_point(center,axis,handle_length,2*kPi*(i+1)/64),rgb(kAxisColors[axis]));
        } else for (int axis=0;axis<3;++axis) for (float sign : {-1.0F,1.0F}) {
            const Vec3 dir=multiply(gizmo_axis(axis),sign);
            const Vec3 handle=add(center,multiply(dir,handle_length));
            const float depth=to_camera(handle,camera,basis).z;
            if (depth<kNearPlane) continue;
            const Vec3 color=rgb(kAxisColors[axis]);
            line(overlay_lines,center,handle,color);
            const float radius=pixel_radius*2.0F*depth*v_tan/static_cast<float>(height);
            const auto triangle=[&](Vec3 a,Vec3 b,Vec3 c) {
                for (auto p:{a,b,c}) overlay_triangles.push_back({p,color});
            };
            if (gizmo_mode==2) {
                std::array<Vec3,8> corners;
                for (int i=0;i<8;++i) corners[i]=add(handle,
                    {(i&1)?radius:-radius,(i&2)?radius:-radius,(i&4)?radius:-radius});
                constexpr int faces[6][4]{{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
                for (const auto& f:faces) {
                    triangle(corners[f[0]],corners[f[1]],corners[f[2]]);
                    triangle(corners[f[0]],corners[f[2]],corners[f[3]]);
                }
            } else {
                const Vec3 u=gizmo_axis((axis+1)%3),v=cross(dir,u);
                const Vec3 tip=add(handle,multiply(dir,radius*1.6F));
                const Vec3 base=subtract(handle,multiply(dir,radius*2));
                const auto rim=[&](int i) {
                    const float angle=2*kPi*i/16;
                    return add(base,add(multiply(u,std::cos(angle)*radius),multiply(v,std::sin(angle)*radius)));
                };
                for (int i=0;i<16;++i) { triangle(tip,rim(i),rim(i+1)); triangle(base,rim(i+1),rim(i)); }
            }
        }
    }
    FrameGeometry frame;
    const std::size_t total=lines.size()+overlay_lines.size()+overlay_triangles.size();
    if (total>std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Scene has too many vertices");
    frame.lines=static_cast<std::uint32_t>(lines.size());
    frame.overlay_lines=static_cast<std::uint32_t>(overlay_lines.size());
    frame.overlay_triangles=static_cast<std::uint32_t>(overlay_triangles.size());
    frame.vertices.reserve(total);
    for (const auto* batch : {&lines,&overlay_lines,&overlay_triangles})
        frame.vertices.insert(frame.vertices.end(),batch->begin(),batch->end());
    frame.geometry=builder.build();
    return frame;
}
} // namespace internal
} // namespace engine
