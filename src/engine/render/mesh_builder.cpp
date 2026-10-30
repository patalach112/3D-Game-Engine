// Primitive meshes, including genuinely flat XY circle/square geometry.
#include "engine/render/internal_types.hpp"
#include <stdexcept>

namespace engine::internal {
MeshData make_primitive_mesh(PrimitiveType type) {
    MeshData mesh;
    if(type==PrimitiveType::Cube) {
        for(int i=0;i<8;++i)
            mesh.vertices.push_back({(i&1)?0.5F:-0.5F,(i&2)?0.5F:-0.5F,(i&4)?0.5F:-0.5F});
        constexpr std::array<std::array<int,4>,6> faces{{
            {{0,1,3,2}},{{4,6,7,5}},{{0,4,5,1}},{{2,3,7,6}},{{0,2,6,4}},{{1,5,7,3}}
        }};
        for(const auto& f:faces) {
            mesh.triangles.push_back({f[0],f[1],f[2]});
            mesh.triangles.push_back({f[0],f[2],f[3]});
        }
        mesh.edges={{{0,1}},{{1,3}},{{3,2}},{{2,0}},{{4,5}},{{5,7}},
                    {{7,6}},{{6,4}},{{0,4}},{{1,5}},{{2,6}},{{3,7}}};
    } else if(type==PrimitiveType::Plane) {
        mesh.vertices={{-0.5F,0,-0.5F},{0.5F,0,-0.5F},{0.5F,0,0.5F},{-0.5F,0,0.5F}};
        mesh.triangles={{{0,1,2}},{{0,2,3}}};
        mesh.edges={{{0,1}},{{1,2}},{{2,3}},{{3,0}}};
    } else if(type==PrimitiveType::Square2D) {
        mesh.vertices={{-0.5F,-0.5F,0},{0.5F,-0.5F,0},{0.5F,0.5F,0},{-0.5F,0.5F,0}};
        mesh.triangles={{{0,1,2}},{{0,2,3}}};
        mesh.edges={{{0,1}},{{1,2}},{{2,3}},{{3,0}}};
    } else if(type==PrimitiveType::Circle2D) {
        constexpr int segments=64;
        mesh.vertices.push_back({0,0,0});
        for(int i=0;i<segments;++i) {
            const float a=2*kPi*static_cast<float>(i)/segments;
            mesh.vertices.push_back({0.5F*std::cos(a),0.5F*std::sin(a),0});
        }
        for(int i=0;i<segments;++i) {
            const int a=i+1,b=(i+1)%segments+1;
            mesh.triangles.push_back({0,a,b});
            mesh.edges.push_back({a,b});
        }
    } else if(type==PrimitiveType::Cylinder) {
        constexpr int segments=20;
        for(int ring=0;ring<2;++ring) for(int i=0;i<segments;++i) {
            const float angle=2.0F*kPi*static_cast<float>(i)/static_cast<float>(segments);
            mesh.vertices.push_back({0.5F*std::cos(angle),ring?0.5F:-0.5F,0.5F*std::sin(angle)});
        }
        const int bottom_center=static_cast<int>(mesh.vertices.size());
        mesh.vertices.push_back({0,-0.5F,0});
        const int top_center=static_cast<int>(mesh.vertices.size());
        mesh.vertices.push_back({0,0.5F,0});
        for(int i=0;i<segments;++i) {
            const int next=(i+1)%segments;
            mesh.triangles.push_back({i,next,segments+next});
            mesh.triangles.push_back({i,segments+next,segments+i});
            mesh.triangles.push_back({bottom_center,next,i});
            mesh.triangles.push_back({top_center,segments+i,segments+next});
            mesh.edges.push_back({i,next});
            mesh.edges.push_back({segments+i,segments+next});
            mesh.edges.push_back({i,segments+i});
        }
    } else {
        constexpr int segments=16, latitude_steps=8;
        mesh.vertices.push_back({0,0.5F,0});
        for(int latitude=1;latitude<latitude_steps;++latitude) {
            const float phi=kPi*static_cast<float>(latitude)/static_cast<float>(latitude_steps);
            const float y=0.5F*std::cos(phi),radius=0.5F*std::sin(phi);
            for(int i=0;i<segments;++i) {
                const float theta=2.0F*kPi*static_cast<float>(i)/static_cast<float>(segments);
                mesh.vertices.push_back({radius*std::cos(theta),y,radius*std::sin(theta)});
            }
        }
        const int bottom=static_cast<int>(mesh.vertices.size()),first_ring=1;
        mesh.vertices.push_back({0,-0.5F,0});
        for(int i=0;i<segments;++i)
            mesh.triangles.push_back({0,first_ring+i,first_ring+(i+1)%segments});
        for(int ring=0;ring<latitude_steps-2;++ring) {
            const int upper=first_ring+ring*segments,lower=upper+segments;
            for(int i=0;i<segments;++i) {
                const int next=(i+1)%segments;
                mesh.triangles.push_back({upper+i,lower+i,upper+next});
                mesh.triangles.push_back({upper+next,lower+i,lower+next});
            }
        }
        const int last_ring=first_ring+(latitude_steps-2)*segments;
        for(int i=0;i<segments;++i)
            mesh.triangles.push_back({bottom,last_ring+(i+1)%segments,last_ring+i});
    }
    if (type == PrimitiveType::Sphere) {
        for (const auto& t : mesh.triangles) {
            mesh.edges.push_back({t[0], t[1]});
            mesh.edges.push_back({t[1], t[2]});
            mesh.edges.push_back({t[2], t[0]});
        }
    }
    return mesh;
}

const MeshData& primitive_mesh(PrimitiveType type) {
    static const std::array<MeshData, 6> cache{
        make_primitive_mesh(PrimitiveType::Cube),
        make_primitive_mesh(PrimitiveType::Sphere),
        make_primitive_mesh(PrimitiveType::Cylinder),
        make_primitive_mesh(PrimitiveType::Plane),
        make_primitive_mesh(PrimitiveType::Circle2D),
        make_primitive_mesh(PrimitiveType::Square2D),
    };
    const auto index=static_cast<std::size_t>(type);
    if (index>=cache.size()) throw std::invalid_argument("Invalid primitive type");
    return cache[index];
}
} // namespace engine::internal
