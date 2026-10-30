#include "engine/scene/model_loader.hpp"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace engine {
namespace {
constexpr std::uintmax_t max_file_bytes=32U*1024U*1024U;
constexpr std::size_t max_vertices=1'000'000,max_triangles=2'000'000;
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
}
std::shared_ptr<const MeshAsset> load_model(const std::filesystem::path& path,std::string& error) {
    try {
        std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);
        if(ec||!bytes||bytes>max_file_bytes){error="Plik modelu nie istnieje, jest pusty lub przekracza 32 MiB";return {};}
        Assimp::Importer importer;
        const auto utf8=path.u8string();
        const std::string source(reinterpret_cast<const char*>(utf8.data()),utf8.size());
        constexpr unsigned flags=aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|aiProcess_PreTransformVertices|
            aiProcess_ImproveCacheLocality|aiProcess_SortByPType|aiProcess_FindDegenerates|aiProcess_FindInvalidData|
            aiProcess_GenSmoothNormals|aiProcess_ValidateDataStructure;
        const aiScene* scene=importer.ReadFile(source,flags);
        if(!scene||!scene->HasMeshes()) {
            error=importer.GetErrorString();if(error.empty())error="Format nie jest obsługiwany albo plik nie zawiera geometrii";return {};
        }
        auto result=std::make_shared<MeshAsset>();result->source=source;
        double color_weight{};Vec3 color_sum{};
        for(unsigned m=0;m<scene->mNumMeshes;++m) {
            const aiMesh* mesh=scene->mMeshes[m];
            if(!mesh||!mesh->HasPositions())continue;
            if(mesh->mNumVertices>max_vertices-result->vertices.size()){error="Model przekracza limit miliona wierzchołków";return {};}
            const int base=static_cast<int>(result->vertices.size());
            for(unsigned i=0;i<mesh->mNumVertices;++i) {
                const auto& p=mesh->mVertices[i];const Vec3 vertex{p.x,p.y,p.z};
                if(!finite(vertex)){error="Model zawiera nieprawidłowe współrzędne";return {};}
                result->vertices.push_back(vertex);
            }
            for(unsigned i=0;i<mesh->mNumFaces;++i) {
                const auto& face=mesh->mFaces[i];if(face.mNumIndices!=3)continue;
                if(result->triangles.size()>=max_triangles){error="Model przekracza limit dwóch milionów trójkątów";return {};}
                result->triangles.push_back({base+static_cast<int>(face.mIndices[0]),base+static_cast<int>(face.mIndices[1]),base+static_cast<int>(face.mIndices[2])});
            }
            if(mesh->mMaterialIndex<scene->mNumMaterials) {
                aiColor3D diffuse;
                if(scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE,diffuse)==AI_SUCCESS&&
                    std::isfinite(diffuse.r)&&std::isfinite(diffuse.g)&&std::isfinite(diffuse.b)) {
                    const double weight=std::max(1U,mesh->mNumFaces);
                    color_sum.x+=static_cast<float>(std::clamp(diffuse.r,0.0F,1.0F)*weight);
                    color_sum.y+=static_cast<float>(std::clamp(diffuse.g,0.0F,1.0F)*weight);
                    color_sum.z+=static_cast<float>(std::clamp(diffuse.b,0.0F,1.0F)*weight);color_weight+=weight;
                }
            }
        }
        if(result->vertices.empty()||result->triangles.empty()){error="Model nie zawiera trójkątnej geometrii";return {};}
        Vec3 lo=result->vertices.front(),hi=lo;
        for(auto v:result->vertices){lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);}
        const Vec3 center{(lo.x+hi.x)*.5F,(lo.y+hi.y)*.5F,(lo.z+hi.z)*.5F};
        const float extent=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
        if(!std::isfinite(extent)||extent<=1e-8F){error="Model ma zerowy rozmiar";return {};}
        const float scale=2.0F/extent;for(auto& v:result->vertices)v={(v.x-center.x)*scale,(v.y-center.y)*scale,(v.z-center.z)*scale};
        std::unordered_set<std::uint64_t> edges;edges.reserve(result->triangles.size()*2);
        for(const auto& triangle:result->triangles)for(int e=0;e<3;++e){auto a=triangle[e],b=triangle[(e+1)%3];if(a>b)std::swap(a,b);const auto key=(static_cast<std::uint64_t>(a)<<32)|static_cast<unsigned>(b);if(edges.insert(key).second)result->edges.push_back({a,b});}
        if(color_weight>0)result->suggested_color={static_cast<float>(color_sum.x/color_weight),static_cast<float>(color_sum.y/color_weight),static_cast<float>(color_sum.z/color_weight)};
        error.clear();return result;
    } catch(const std::exception& exception){error=exception.what();return {};}
}
std::shared_ptr<const MeshAsset> load_obj(const std::filesystem::path& path,std::string& error) {
    return load_model(path,error);
}
}
