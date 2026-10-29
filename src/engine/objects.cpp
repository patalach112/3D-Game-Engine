#include "engine/objects.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace engine {
namespace {
bool finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
void require(bool valid,const char* message) { if (!valid) throw std::invalid_argument(message); }
std::uint32_t count(std::size_t size) {
    if (size>static_cast<std::size_t>(INT32_MAX)) throw std::length_error("Geometry exceeds signed base-vertex/index limit");
    return static_cast<std::uint32_t>(size);
}
std::size_t add(std::size_t a,std::size_t b) {
    if (b>std::numeric_limits<std::size_t>::max()-a) throw std::length_error("Geometry byte size overflow");
    return a+b;
}
std::size_t product(std::size_t n,std::size_t stride) {
    if (n>std::numeric_limits<std::size_t>::max()/stride) throw std::length_error("Geometry section overflow");
    return n*stride;
}
GeometryLayout layout(std::size_t hv,std::size_t sv,std::size_t hi,std::size_t si) {
    GeometryLayout l{};
    l.hudVertexCount=count(hv); l.sceneVertexCount=count(sv);
    l.hudIndexCount=count(hi); l.sceneIndexCount=count(si);
    l.sceneVertices=product(hv,sizeof(Vertex_hud));
    l.hudIndices=add(l.sceneVertices,product(sv,sizeof(Vertex)));
    l.sceneIndices=add(l.hudIndices,product(hi,sizeof(std::uint32_t)));
    l.totalBytes=add(l.sceneIndices,product(si,sizeof(std::uint32_t)));
    return l;
}
template<class T> std::vector<T> extract(const GeometryBuffer& b,std::size_t offset,std::size_t n) {
    std::vector<T> out(n);
    if (n) std::memcpy(out.data(),b.bytes.data()+offset,n*sizeof(T));
    return out;
}
}
std::uint16_t float_to_half(float value) {
    require(std::isfinite(value)&&std::abs(value)<=65504,"Vertex position outside finite binary16 range");
    const auto bits=std::bit_cast<std::uint32_t>(value);
    const auto sign=(bits>>16)&0x8000U;
    const int exponent=static_cast<int>((bits>>23)&255)-127+15;
    const auto mantissa=bits&0x7FFFFFU;
    if (exponent<-10) return static_cast<std::uint16_t>(sign);
    if (exponent<=0) {
        const auto m=mantissa|0x800000U;
        const unsigned shift=static_cast<unsigned>(14-exponent);
        auto result=m>>shift;
        const auto remainder=m&((1U<<shift)-1);
        const auto halfway=1U<<(shift-1);
        if (remainder>halfway || (remainder==halfway && (result&1))) ++result;
        return static_cast<std::uint16_t>(sign|result);
    }
    auto result=(static_cast<unsigned>(exponent)<<10)|(mantissa>>13);
    const auto remainder=mantissa&8191;
    if (remainder>4096 || (remainder==4096 && (result&1))) ++result;
    return static_cast<std::uint16_t>(sign|result);
}
float half_to_float(std::uint16_t value) noexcept {
    const auto sign=static_cast<std::uint32_t>(value&0x8000)<<16;
    const auto e=(value>>10)&31;
    auto m=static_cast<std::uint32_t>(value&1023);
    if (!e) {
        if (!m) return std::bit_cast<float>(sign);
        int exponent=-14;
        while (!(m&1024)) { m<<=1; --exponent; }
        return std::bit_cast<float>(sign|(static_cast<std::uint32_t>(exponent+127)<<23)|((m&1023)<<13));
    }
    return std::bit_cast<float>(sign|((e==31?255U:static_cast<unsigned>(e)+112U)<<23)|(m<<13));
}
std::uint32_t pack_normal(Vec3 v) {
    require(finite(v),"Non-finite normal");
    const double length=std::hypot(static_cast<double>(v.x),v.y,v.z);
    require(length>0,"Normal must not be zero");
    const auto channel=[&](float x) { return static_cast<std::uint32_t>(std::lround((x/length*0.5+0.5)*1023)); };
    return channel(v.x)|(channel(v.y)<<10)|(channel(v.z)<<20)|(3U<<30);
}
Vec3 unpack_normal(std::uint32_t p) noexcept {
    return {static_cast<float>(p&1023)/1023*2-1,static_cast<float>((p>>10)&1023)/1023*2-1,
        static_cast<float>((p>>20)&1023)/1023*2-1};
}
Vertex make_vertex(Vec3 p,Vec3 n) {
    return {{float_to_half(p.x),float_to_half(p.y),float_to_half(p.z),float_to_half(1)},pack_normal(n)};
}
std::int32_t pack_rotation(Vec3 d) {
    require(finite(d),"Non-finite rotation");
    const auto axis=[](float degrees) { double turn=std::fmod(static_cast<double>(degrees),360.0)/360;
        if (turn<0) turn+=1;
        return static_cast<std::uint32_t>(std::lround(turn*1024))&1023; };
    return static_cast<std::int32_t>(axis(d.x)|(axis(d.y)<<10)|(axis(d.z)<<20));
}
Vec3 unpack_rotation(std::int32_t p) noexcept {
    const auto u=static_cast<std::uint32_t>(p);
    return {static_cast<float>(u&1023)*360/1024,static_cast<float>((u>>10)&1023)*360/1024,
        static_cast<float>((u>>20)&1023)*360/1024};
}
void GeometryBuilder::add_hud(std::span<const Vertex_hud> v,std::span<const std::uint32_t> i) {
    require(i.size()%3==0,"HUD needs triangle indices");
    const auto base=count(hud_.size()); count(add(hud_.size(),v.size())); count(add(hudIndices_.size(),i.size()));
    for (auto index:i) require(index<v.size(),"HUD index outside mesh");
    for (auto vertex:v) require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y)
        &&std::isfinite(vertex.color.x)&&std::isfinite(vertex.color.y)&&std::isfinite(vertex.color.z)&&std::isfinite(vertex.color.w),"Non-finite HUD vertex");
    hud_.insert(hud_.end(),v.begin(),v.end());
    for (auto index:i) hudIndices_.push_back(index+base);
}
void GeometryBuilder::append_ui(std::span<const Vertex_hud> v,std::span<const std::uint32_t> i) {
    // ImGui draw commands supply list-local baseVertex; don't rebase their indices.
    require(i.size()%3==0,"UI needs triangle indices");
    count(add(hud_.size(),v.size())); count(add(hudIndices_.size(),i.size()));
    hud_.insert(hud_.end(),v.begin(),v.end()); hudIndices_.insert(hudIndices_.end(),i.begin(),i.end());
}
MeshId GeometryBuilder::add_mesh(std::span<const Vertex> v,std::span<const std::uint32_t> i) {
    require(!v.empty()&&!i.empty()&&i.size()%3==0,"3D mesh needs indexed triangles");
    count(add(vertices_.size(),v.size())); count(add(indices_.size(),i.size()));
    double radius=0;
    for (auto vertex:v) {
        const Vec3 p{half_to_float(vertex.pos.x),half_to_float(vertex.pos.y),half_to_float(vertex.pos.z)};
        require(finite(p)&&half_to_float(vertex.pos.w)==1,"Invalid half position (w must be 1)");
        radius=std::max(radius,std::hypot(static_cast<double>(p.x),p.y,p.z));
    }
    for (auto index:i) require(index<v.size(),"3D index outside mesh");
    const Mesh mesh{count(indices_.size()),count(i.size()),static_cast<std::int32_t>(count(vertices_.size())),std::nextafter(static_cast<float>(radius),std::numeric_limits<float>::infinity())};
    const auto id=count(meshes_.size());
    vertices_.insert(vertices_.end(),v.begin(),v.end()); indices_.insert(indices_.end(),i.begin(),i.end());
    meshes_.push_back(mesh); return id;
}
std::uint32_t GeometryBuilder::add_object(MeshId id,ObjectData data,Vec4 tint) {
    require(id<meshes_.size(),"Unknown mesh");
    require(finite(data.position)&&std::isfinite(data.scale)&&std::isfinite(data.radius)&&data.radius>=0,"Invalid object transform/radius");
    require((static_cast<std::uint32_t>(data.rotation)&0xC0000000U)==0,"Reserved rotation bits must be zero");
    require(data.padding==0,"ObjectData padding must be zero");
    require(std::isfinite(tint.x)&&std::isfinite(tint.y)&&std::isfinite(tint.z)&&std::isfinite(tint.w),"Non-finite object tint");
    const auto& mesh=meshes_[id];
    const double required=static_cast<double>(mesh.radius)*std::abs(data.scale);
    require(std::isfinite(required)&&required<=std::numeric_limits<float>::max(),"Object radius overflow");
    const float minimum=std::nextafter(static_cast<float>(required),std::numeric_limits<float>::infinity());
    require(std::isfinite(minimum),"Object radius overflow");
    if (!data.radius) data.radius=minimum;
    require(data.radius>=minimum,"Bounding sphere too small for mesh and scale");
    const auto instance=count(objects_.size());
    objects_.push_back(data); tints_.push_back(tint);
    commands_.push_back({mesh.count,1,mesh.firstIndex,mesh.baseVertex,instance}); return instance;
}
GeometryBuffer GeometryBuilder::build() const {
    GeometryBuffer out;
    out.layout=layout(hud_.size(),vertices_.size(),hudIndices_.size(),indices_.size()); out.bytes.resize(out.layout.totalBytes);
    const auto copy=[&](std::size_t offset,const auto& source) { if (!source.empty()) std::memcpy(out.bytes.data()+offset,source.data(),source.size()*sizeof(source[0])); };
    copy(0,hud_); copy(out.layout.sceneVertices,vertices_); copy(out.layout.hudIndices,hudIndices_); copy(out.layout.sceneIndices,indices_);
    out.objects=objects_; out.commands=commands_; out.tints=tints_; return out;
}
GeometryBuilder GeometryBuilder::from_buffer(const GeometryBuffer& b) {
    b.validate(); GeometryBuilder out;
    out.hud_=extract<Vertex_hud>(b,b.layout.hudVertices,b.layout.hudVertexCount);
    out.vertices_=extract<Vertex>(b,b.layout.sceneVertices,b.layout.sceneVertexCount);
    out.hudIndices_=extract<std::uint32_t>(b,b.layout.hudIndices,b.layout.hudIndexCount);
    out.indices_=extract<std::uint32_t>(b,b.layout.sceneIndices,b.layout.sceneIndexCount);
    out.objects_=b.objects; out.commands_=b.commands; out.tints_=b.tints;
    return out; // Can append HUD; mesh handles intentionally aren't reconstructed.
}
void GeometryBuffer::validate() const {
    const auto expected=engine::layout(layout.hudVertexCount,layout.sceneVertexCount,layout.hudIndexCount,layout.sceneIndexCount);
    require(layout.hudVertices==0&&layout.sceneVertices==expected.sceneVertices&&layout.hudIndices==expected.hudIndices
        &&layout.sceneIndices==expected.sceneIndices&&layout.totalBytes==expected.totalBytes&&bytes.size()==layout.totalBytes,"Invalid geometry section layout");
    count(objects.size());
    require(objects.size()==commands.size()&&objects.size()==tints.size(),"Object/command/tint count mismatch");
    const auto indices=extract<std::uint32_t>(*this,layout.sceneIndices,layout.sceneIndexCount);
    for (std::size_t n=0;n<commands.size();++n) {
        const auto c=commands[n]; const auto o=objects[n];
        require(c.instanceCount<=1&&c.firstInstance==n&&c.indexCount%3==0&&c.firstIndex<=indices.size()
            &&c.indexCount<=indices.size()-c.firstIndex,"Invalid indirect command");
        require(finite(o.position)&&std::isfinite(o.scale)&&std::isfinite(o.radius)&&o.radius>=0&&o.padding==0
            &&(static_cast<std::uint32_t>(o.rotation)&0xC0000000U)==0,"Invalid ObjectData");
        const auto tint=tints[n];
        require(std::isfinite(tint.x)&&std::isfinite(tint.y)&&std::isfinite(tint.z)&&std::isfinite(tint.w),"Non-finite tint");
        double radius=0;
        for (std::size_t j=c.firstIndex;j<static_cast<std::size_t>(c.firstIndex)+c.indexCount;++j) {
            const auto vertex=static_cast<std::int64_t>(indices[j])+c.vertexOffset;
            require(vertex>=0&&vertex<layout.sceneVertexCount,"Indirect command vertex outside geometry");
            Vertex v{}; std::memcpy(&v,bytes.data()+layout.sceneVertices+static_cast<std::size_t>(vertex)*sizeof(Vertex),sizeof(v));
            const Vec3 p{half_to_float(v.pos.x),half_to_float(v.pos.y),half_to_float(v.pos.z)};
            require(finite(p)&&half_to_float(v.pos.w)==1,"Invalid object vertex");
            radius=std::max(radius,std::hypot(static_cast<double>(p.x),p.y,p.z));
        }
        require(static_cast<double>(o.radius)>=radius*std::abs(o.scale),"Object radius doesn't enclose mesh");
    }
    const auto hud=extract<Vertex_hud>(*this,layout.hudVertices,layout.hudVertexCount);
    for (auto v:hud) require(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y)&&std::isfinite(v.color.x)
        &&std::isfinite(v.color.y)&&std::isfinite(v.color.z)&&std::isfinite(v.color.w),"Invalid HUD vertex");
    const auto hi=extract<std::uint32_t>(*this,layout.hudIndices,layout.hudIndexCount);
    for (auto index:hi) require(index<layout.hudVertexCount,"HUD index outside geometry");
}
} // namespace engine
