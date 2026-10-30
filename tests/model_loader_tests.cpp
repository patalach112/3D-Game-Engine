#include "engine/scene/model_loader.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    const auto path=std::filesystem::temp_directory_path()/"starter_engine_import_test.obj";
    {
        std::ofstream out(path);
        out<<"# quad with texture/normal syntax and negative indices\n"
              "v -2 0 -1\n"
              "v 2 0 -1\n"
              "v 2 0 1\n"
              "v -2 0 1\n"
              "f -4 -3 -2 -1\n";
    }
    std::string error;auto mesh=engine::load_model(path,error);
    assert(mesh && error.empty());
    assert(mesh->vertices.size()==4 && mesh->triangles.size()==2 && mesh->edges.size()==5);
    float extent=0;
    for(auto v:mesh->vertices)extent=std::max({extent,std::abs(v.x),std::abs(v.y),std::abs(v.z)});
    assert(std::abs(extent-1.0F)<0.0001F);

    const auto stl_path=std::filesystem::temp_directory_path()/"starter_engine_import_test.stl";
    {std::ofstream out(stl_path);out<<"solid test\nfacet normal 0 0 1\nouter loop\n"
        "vertex 0 0 0\nvertex 2 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid test\n";}
    auto stl=engine::load_model(stl_path,error);
    assert(stl && stl->triangles.size()==1 && stl->vertices.size()==3 && stl->edges.size()==3);
    std::filesystem::remove(stl_path);

    {std::ofstream out(path);out<<"v 0 0 0\nf 1 2 3\n";}
    assert(!engine::load_model(path,error) && !error.empty());
    std::filesystem::remove(path);
    std::cout<<"OBJ and STL import, validation and normalization passed\n";
}
