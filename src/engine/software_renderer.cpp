#include "engine/render/software_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace engine {
namespace {
constexpr float kPi = 3.14159265358979323846F;
constexpr float kNearPlane = 0.08F;
constexpr std::uint32_t kBackground = 0x00272729U;
constexpr std::array<std::uint32_t, 3> kAxisColors{0x00E84C4CU, 0x005ACB70U, 0x004E91F2U};

Vec3 add(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 multiply(Vec3 a,float n) { return {a.x*n,a.y*n,a.z*n}; }
float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float length(Vec3 v) { return std::sqrt(dot(v,v)); }
Vec3 normalize(Vec3 v) {
    const float magnitude=length(v);
    return magnitude<1.0e-6F?Vec3{0,0,1}:multiply(v,1.0F/magnitude);
}

struct Basis { Vec3 right,up,forward; };
Basis camera_basis(const EditorCamera& camera) {
    const Vec3 forward=normalize(subtract(camera.target,camera.position));
    const Vec3 right=normalize(cross(forward,{0,1,0}));
    return {right,normalize(cross(right,forward)),forward};
}
struct CameraPoint { float x,y,z; };
CameraPoint to_camera(Vec3 world,const EditorCamera& camera,const Basis& basis) {
    const Vec3 delta=subtract(world,camera.position);
    return {dot(delta,basis.right),dot(delta,basis.up),dot(delta,basis.forward)};
}
std::uint32_t shade(std::uint32_t color,float factor) {
    const auto channel=[color,factor](std::uint32_t shift) {
        return static_cast<std::uint32_t>(std::clamp(static_cast<int>(((color>>shift)&255U)*factor),0,255));
    };
    return (channel(16)<<16U)|(channel(8)<<8U)|channel(0);
}

struct MeshData {
    std::vector<Vec3> vertices;
    std::vector<std::array<int,3>> triangles;
    std::vector<std::array<int,2>> edges;
};

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
    return mesh;
}
} // namespace

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

void SoftwareRenderer::resize(int width,int height) {
    width_=std::max(1,width); height_=std::max(1,height);
    const auto count=static_cast<std::size_t>(width_)*static_cast<std::size_t>(height_);
    pixels_.resize(count); depth_.resize(count);
}

void SoftwareRenderer::render(const Scene& scene,const EditorCamera& camera,EntityId selected) {
    std::fill(pixels_.begin(),pixels_.end(),kBackground);
    std::fill(depth_.begin(),depth_.end(),std::numeric_limits<float>::infinity());
    const Basis basis=camera_basis(camera);
    const float focal=(static_cast<float>(height_)*0.5F)/std::tan(camera.vertical_fov_degrees*kPi/360.0F);
    // Project from camera space. Values that overflow or land behind the near
    // plane are reported as invisible instead of producing NaN screen coords.
    const auto project_camera=[&](const CameraPoint& p)->ScreenPoint {
        if(p.z<kNearPlane||!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) return {};
        const float sx=static_cast<float>(width_)*0.5F+p.x*focal/p.z;
        const float sy=static_cast<float>(height_)*0.5F-p.y*focal/p.z;
        if(!std::isfinite(sx)||!std::isfinite(sy)) return {};
        return {sx,sy,p.z,true};
    };
    const auto project=[&](Vec3 world)->ScreenPoint {
        return project_camera(to_camera(world,camera,basis));
    };
    const auto put_pixel=[&](int x,int y,float z,std::uint32_t color,bool depth_test) {
        if(x<0||x>=width_||y<0||y>=height_) return;
        const auto index=static_cast<std::size_t>(y)*static_cast<std::size_t>(width_)+static_cast<std::size_t>(x);
        if(!depth_test) { pixels_[index]=color; return; }
        if(z<depth_[index]) { pixels_[index]=color; depth_[index]=z; return; }
        if(std::isfinite(depth_[index])) {
            const float tolerance=std::max(0.0005F,depth_[index]*0.00001F);
            if(z<=depth_[index]+tolerance) pixels_[index]=color;
        }
    };
    const auto draw_line=[&](Vec3 a,Vec3 b,std::uint32_t color,bool depth_test,int thickness,bool fade) {
        CameraPoint ca=to_camera(a,camera,basis),cb=to_camera(b,camera,basis);
        if(ca.z<kNearPlane&&cb.z<kNearPlane) return;
        const float near_clip=kNearPlane+0.002F;
        if(ca.z<near_clip) {
            const float t=(near_clip-ca.z)/(cb.z-ca.z);
            a=add(a,multiply(subtract(b,a),t));
        } else if(cb.z<near_clip) {
            const float t=(near_clip-ca.z)/(cb.z-ca.z);
            b=add(a,multiply(subtract(b,a),t));
        }
        const ScreenPoint pa=project(a),pb=project(b);
        if(!pa.visible||!pb.visible) return;
        const float full_dx=pb.x-pa.x,full_dy=pb.y-pa.y;
        const float inv_start_full=1.0F/pa.depth,inv_end_full=1.0F/pb.depth;
        float t0=0.0F,t1=1.0F;
        const auto clip_test=[&](float p,float q) {
            if(std::abs(p)<1.0e-8F) return q>=0.0F;
            const float ratio=q/p;
            if(p<0.0F) { if(ratio>t1) return false; t0=std::max(t0,ratio); }
            else { if(ratio<t0) return false; t1=std::min(t1,ratio); }
            return true;
        };
        if(!clip_test(-full_dx,pa.x)||!clip_test(full_dx,static_cast<float>(width_-1)-pa.x)
          ||!clip_test(-full_dy,pa.y)||!clip_test(full_dy,static_cast<float>(height_-1)-pa.y)||t0>t1) return;
        const auto at=[&](float t) {
            const float inv=inv_start_full+(inv_end_full-inv_start_full)*t;
            return ScreenPoint{pa.x+full_dx*t,pa.y+full_dy*t,1.0F/inv,true};
        };
        const ScreenPoint first=at(t0),last=at(t1);
        const float dx=last.x-first.x,dy=last.y-first.y;
        const float inv_start=1.0F/first.depth,inv_end=1.0F/last.depth;
        const int steps=std::max(1,static_cast<int>(std::ceil(std::max(std::abs(dx),std::abs(dy)))));
        for(int i=0;i<=steps;++i) {
            const float t=static_cast<float>(i)/static_cast<float>(steps);
            const float z=1.0F/(inv_start+(inv_end-inv_start)*t);
            std::uint32_t sample=color;
            if(fade) {
                const float strength=std::clamp(1.0F-std::max(0.0F,z-12.0F)/85.0F,0.42F,1.0F);
                const auto mix=[strength](std::uint32_t fg,std::uint32_t bg,int shift) {
                    return static_cast<std::uint32_t>(((fg>>shift)&255U)*strength+((bg>>shift)&255U)*(1-strength));
                };
                sample=(mix(color,kBackground,16)<<16U)|(mix(color,kBackground,8)<<8U)|mix(color,kBackground,0);
            }
            const int px=static_cast<int>(std::lround(first.x+dx*t));
            const int py=static_cast<int>(std::lround(first.y+dy*t));
            for(int oy=0;oy<thickness;++oy) for(int ox=0;ox<thickness;++ox)
                put_pixel(px+ox,py+oy,z,sample,depth_test);
        }
    };

    // The default ground is a transparent grid: only its lines are rasterized.
    constexpr int grid_extent=28;
    for(int i=-grid_extent;i<=grid_extent;++i) {
        const auto color=(i%5==0)?0x00525255U:0x00373739U;
        const float coordinate=static_cast<float>(i);
        draw_line({-static_cast<float>(grid_extent),0,coordinate},{static_cast<float>(grid_extent),0,coordinate},color,true,2,true);
        draw_line({coordinate,0,-static_cast<float>(grid_extent)},{coordinate,0,static_cast<float>(grid_extent)},color,true,2,true);
    }

    const Vec3 light=normalize({-0.4F,0.8F,0.55F});
    for(const Entity& entity:scene.entities()) {
        const MeshData mesh=make_primitive_mesh(entity.primitive);
        const Vec3 scale=entity.transform.scale;
        const float rx=entity.transform.rotation.x*kPi/180.0F;
        const float ry=entity.transform.rotation.y*kPi/180.0F;
        const float rz=entity.transform.rotation.z*kPi/180.0F;
        const float cx=std::cos(rx),sx=std::sin(rx),cy=std::cos(ry),sy=std::sin(ry),cz=std::cos(rz),sz=std::sin(rz);
        std::vector<Vec3> world(mesh.vertices.size());
        std::vector<CameraPoint> camera_space(mesh.vertices.size());
        for(std::size_t i=0;i<mesh.vertices.size();++i) {
            Vec3 p=mesh.vertices[i]; p={p.x*scale.x,p.y*scale.y,p.z*scale.z};
            const Vec3 px{p.x,p.y*cx-p.z*sx,p.y*sx+p.z*cx};
            const Vec3 py{px.x*cy+px.z*sy,px.y,-px.x*sy+px.z*cy};
            const Vec3 pz{py.x*cz-py.y*sz,py.x*sz+py.y*cz,py.z};
            world[i]=add(pz,entity.transform.position);
            camera_space[i]=to_camera(world[i],camera,basis);
        }
        const bool is_selected=entity.id==selected;
        std::uint32_t base=entity.primitive==PrimitiveType::Sphere?0x007FA8D8U:
            entity.primitive==PrimitiveType::Cylinder?0x007DAF82U:
            entity.primitive==PrimitiveType::Plane?0x00B2A27DU:0x00A7A7AAU;
        if(is_selected) base=0x00D8D8DAU;

        // Przeciągamy trójkąty przez płaszczyznę bliskości zamiast ich odrzucać:
        // inaczej duży obiekt, w którym kamera się znajduje, traci każdą ścianę,
        // której wierzchołki leżą po dwóch stronach płaszczyzny bliskości.
        const auto fill_triangle=[&](const ScreenPoint& p0,const ScreenPoint& p1,const ScreenPoint& p2,
                                     std::uint32_t fill_color) {
            const float area=(p1.x-p0.x)*(p2.y-p0.y)-(p1.y-p0.y)*(p2.x-p0.x);
            if(std::abs(area)<1.0e-5F) return;
            const float x_min=std::min({p0.x,p1.x,p2.x}),x_max=std::max({p0.x,p1.x,p2.x});
            const float y_min=std::min({p0.y,p1.y,p2.y}),y_max=std::max({p0.y,p1.y,p2.y});
            if(!std::isfinite(x_min)||!std::isfinite(x_max)||!std::isfinite(y_min)||!std::isfinite(y_max)) return;
            // Przyciągamy granice do zakresu int przed konwersją; wynik jest ten
            // sam, bo skanowanie jest i tak ograniczone do viewporta.
            constexpr float kLimit=1.0e7F;
            const int min_x=std::max(0,static_cast<int>(std::floor(std::max(x_min,-kLimit))));
            const int max_x=std::min(width_-1,static_cast<int>(std::ceil(std::min(x_max,kLimit))));
            const int min_y=std::max(0,static_cast<int>(std::floor(std::max(y_min,-kLimit))));
            const int max_y=std::min(height_-1,static_cast<int>(std::ceil(std::min(y_max,kLimit))));
            for(int y=min_y;y<=max_y;++y) for(int x=min_x;x<=max_x;++x) {
                const float px=static_cast<float>(x)+0.5F,py=static_cast<float>(y)+0.5F;
                const float w0=((p1.x-px)*(p2.y-py)-(p1.y-py)*(p2.x-px))/area;
                const float w1=((p2.x-px)*(p0.y-py)-(p2.y-py)*(p0.x-px))/area;
                const float w2=1.0F-w0-w1;
                if(w0 < -0.001F||w1 < -0.001F||w2 < -0.001F) continue;
                const float inv=w0/p0.depth+w1/p1.depth+w2/p2.depth;
                if(!std::isfinite(inv)||inv<=0.0F) continue;
                put_pixel(x,y,1.0F/inv,fill_color,true);
            }
        };

        for(const auto& tri:mesh.triangles) {
            const int ia=tri[0],ib=tri[1],ic=tri[2];
            const CameraPoint ca=camera_space[static_cast<std::size_t>(ia)];
            const CameraPoint cb=camera_space[static_cast<std::size_t>(ib)];
            const CameraPoint cc=camera_space[static_cast<std::size_t>(ic)];

            // Sutherland-Hodgman przeciwko płaszczyznie bliskości: zachowana część
            // trójkąta to wypukły wielokąt o 3..6 wierzchołkach.
            std::array<CameraPoint,6> poly{};
            int count=0;
            const auto clip_edge=[&](const CameraPoint& a,const CameraPoint& b) {
                const bool a_inside=a.z>=kNearPlane,b_inside=b.z>=kNearPlane;
                if(a_inside&&b_inside) {
                    if(count<6) poly[count++]=b;
                } else if(a_inside&&!b_inside) {
                    const float t=(kNearPlane-a.z)/(b.z-a.z);
                    if(count<6) poly[count++]={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,kNearPlane};
                } else if(!a_inside&&b_inside) {
                    const float t=(kNearPlane-a.z)/(b.z-a.z);
                    if(count<6) poly[count++]={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,kNearPlane};
                    if(count<6) poly[count++]=b;
                }
            };
            clip_edge(ca,cb);
            clip_edge(cb,cc);
            clip_edge(cc,ca);
            if(count<3) continue;

            const Vec3 normal=normalize(cross(subtract(world[static_cast<std::size_t>(ib)],world[static_cast<std::size_t>(ia)]),
                                               subtract(world[static_cast<std::size_t>(ic)],world[static_cast<std::size_t>(ia)])));
            const std::uint32_t color=shade(base,0.48F+0.52F*std::abs(dot(normal,light)));

            std::array<ScreenPoint,6> sp{};
            for(int i=0;i<count;++i) sp[i]=project_camera(poly[i]);
            for(int i=1;i+1<count;++i) fill_triangle(sp[0],sp[i],sp[i+1],color);
        }
        const bool show_edges=entity.primitive==PrimitiveType::Cube||(is_selected&&entity.primitive!=PrimitiveType::Sphere);
        if(show_edges) for(const auto& edge:mesh.edges) {
            draw_line(world[static_cast<std::size_t>(edge[0])],world[static_cast<std::size_t>(edge[1])],
                      is_selected?0x00F1A06BU:0x00555558U,true,1,false);
        }
    }

    if(const Entity* entity=scene.find_entity(selected)) {
        const Vec3 center=entity->transform.position;
        const float distance=length(subtract(camera.position,center));
        const float handle_length=std::clamp(distance*0.13F,0.8F,2.0F);
        for(int axis=0;axis<3;++axis) for(float sign:{-1.0F,1.0F}) {
            const Vec3 direction=multiply(axis==0?Vec3{1,0,0}:axis==1?Vec3{0,1,0}:Vec3{0,0,1},sign);
            const Vec3 handle=add(center,multiply(direction,handle_length));
            const ScreenPoint b=project(handle);
            if(!b.visible) continue;
            draw_line(center,handle,kAxisColors[static_cast<std::size_t>(axis)],false,1,false);
            const int radius=std::clamp(static_cast<int>(std::lround(static_cast<float>(height_)*0.011F)),7,12);
            for(int y=-radius;y<=radius;++y) for(int x=-radius;x<=radius;++x)
                if(x*x+y*y<=radius*radius)
                    put_pixel(static_cast<int>(std::lround(b.x))+x,static_cast<int>(std::lround(b.y))+y,b.depth,
                              kAxisColors[static_cast<std::size_t>(axis)],false);
        }
    }
}

} // namespace engine
