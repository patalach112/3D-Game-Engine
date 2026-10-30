#include "engine/scene/obj_loader.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {
constexpr std::uintmax_t max_file_bytes=32U*1024U*1024U;
constexpr std::size_t max_vertices=1'000'000,max_triangles=2'000'000;
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool index(const std::string& token,std::size_t count,int& result) {
    const auto slash=token.find('/');const auto text=token.substr(0,slash);
    if(text.empty())return false;
    try {
        std::size_t used{};const long long value=std::stoll(text,&used);
        if(used!=text.size()||!value)return false;
        const long long resolved=value>0?value-1:static_cast<long long>(count)+value;
        if(resolved<0||resolved>=static_cast<long long>(count))return false;
        result=static_cast<int>(resolved);return true;
    } catch(...) {return false;}
}
}
std::shared_ptr<const MeshAsset> load_obj(const std::filesystem::path& path,std::string& error) {
    try {
        std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);
        if(ec||!bytes||bytes>max_file_bytes){error="Plik OBJ nie istnieje, jest pusty lub przekracza 32 MiB";return {};}
        std::ifstream file(path);if(!file){error="Nie można otworzyć pliku OBJ";return {};}
        auto mesh=std::make_shared<MeshAsset>();mesh->source=path.string();
        std::string line;std::size_t line_number{};
        while(std::getline(file,line)) {
            ++line_number;if(line.size()>65536){error="Zbyt długa linia OBJ";return {};}
            std::istringstream in(line);std::string kind;if(!(in>>kind)||kind[0]=='#')continue;
            if(kind=="v") {
                Vec3 v;if(!(in>>v.x>>v.y>>v.z)||!finite(v)){error="Nieprawidłowy wierzchołek OBJ w linii "+std::to_string(line_number);return {};}
                if(mesh->vertices.size()>=max_vertices){error="OBJ przekracza limit miliona wierzchołków";return {};}
                mesh->vertices.push_back(v);
            } else if(kind=="f") {
                std::vector<int> face;std::string token;
                while(in>>token){int i{};if(!index(token,mesh->vertices.size(),i)){error="Nieprawidłowa ściana OBJ w linii "+std::to_string(line_number);return {};}face.push_back(i);}
                if(face.size()<3){error="Ściana OBJ ma mniej niż 3 wierzchołki";return {};}
                for(std::size_t i=1;i+1<face.size();++i) {
                    if(mesh->triangles.size()>=max_triangles){error="OBJ przekracza limit dwóch milionów trójkątów";return {};}
                    mesh->triangles.push_back({face[0],face[i],face[i+1]});
                }
            }
        }
        if(!file.eof()){error="Błąd odczytu pliku OBJ";return {};}
        if(mesh->vertices.empty()||mesh->triangles.empty()){error="OBJ nie zawiera geometrii";return {};}
        Vec3 lo=mesh->vertices.front(),hi=lo;
        for(auto v:mesh->vertices){lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);}
        const Vec3 center{(lo.x+hi.x)*.5F,(lo.y+hi.y)*.5F,(lo.z+hi.z)*.5F};
        const float extent=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
        if(!std::isfinite(extent)||extent<=1e-8F){error="OBJ ma zerowy rozmiar";return {};}
        const float scale=2.0F/extent;for(auto& v:mesh->vertices)v={(v.x-center.x)*scale,(v.y-center.y)*scale,(v.z-center.z)*scale};
        std::unordered_set<std::uint64_t> edges;edges.reserve(mesh->triangles.size()*2);
        for(const auto& t:mesh->triangles)for(int e=0;e<3;++e){auto a=t[e],b=t[(e+1)%3];if(a>b)std::swap(a,b);const auto key=(static_cast<std::uint64_t>(a)<<32)|b;if(edges.insert(key).second)mesh->edges.push_back({a,b});}
        error.clear();return mesh;
    } catch(const std::exception& e){error=e.what();return {};}
}
}
