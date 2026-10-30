#include "editor_app.hpp"
#include "engine/log.hpp"
#include "engine/scene/model_loader.hpp"
#include <commdlg.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>

namespace editor {

void save_scene() {
    std::ofstream file("scene.txt");
    if (!file) {
        app().status = L"Nie udało się zapisać sceny";
        LOG_ERROR("editor","Nie udało się otworzyć scene.txt");
        return;
    }
    file << "# Starter 3D Engine scene v5 (name, primitive, transform, RGB, optional model source)\n";
    for (const auto& e : app().scene.entities()) {
        file << e.id << ' ' << std::quoted(e.name) << ' ' << static_cast<int>(e.primitive) << ' '
             << e.transform.position.x << ' ' << e.transform.position.y << ' '
             << e.transform.position.z << ' ' << e.transform.rotation.x << ' ' << e.transform.rotation.y << ' '
             << e.transform.rotation.z << ' ' << e.transform.scale.x << ' ' << e.transform.scale.y << ' '
             << e.transform.scale.z << ' ' << e.color.x << ' ' << e.color.y << ' ' << e.color.z << ' '
             << std::quoted(e.mesh?e.mesh->source:std::string{}) << '\n';
    }
    file.flush();
    if (file) LOG_INFO("editor","Zapisano scenę do scene.txt");
    else LOG_ERROR("editor","Błąd zapisu scene.txt");
    app().status = file?L"Zapisano scenę do scene.txt":L"Błąd zapisu sceny";
}

void import_model() {
    auto& g=app();
    std::vector<wchar_t> path(32768);
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=g.hwnd;
    dialog.lpstrFilter=L"Modele 3D (*.fbx;*.gltf;*.glb;*.dae;*.3ds;*.stl;*.ply;*.obj;*.blend)\0*.fbx;*.gltf;*.glb;*.dae;*.3ds;*.stl;*.ply;*.obj;*.blend\0Wszystkie obsługiwane pliki (*.*)\0*.*\0\0";
    dialog.lpstrFile=path.data();dialog.nMaxFile=static_cast<DWORD>(path.size());
    dialog.lpstrTitle=L"Importuj model 3D (Assimp)";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&dialog)) {
        if(CommDlgExtendedError())g.status=L"Nie udało się otworzyć okna importu";
        return;
    }
    const std::filesystem::path source(path.data());std::string error;
    auto mesh=engine::load_model(source,error);
    if(!mesh){g.status=L"Błąd importu modelu";LOG_ERROR("import","%s",error.c_str());MessageBoxA(g.hwnd,error.c_str(),"Import modelu 3D",MB_OK|MB_ICONERROR);return;}
    const auto wide_name=source.stem().wstring();
    const int bytes=WideCharToMultiByte(CP_UTF8,0,wide_name.data(),static_cast<int>(wide_name.size()),nullptr,0,nullptr,nullptr);
    std::string name(static_cast<std::size_t>(std::max(0,bytes)),'\0');
    if(bytes)WideCharToMultiByte(CP_UTF8,0,wide_name.data(),static_cast<int>(wide_name.size()),name.data(),bytes,nullptr,nullptr);
    if(name.empty())name="Imported model";
    auto& entity=g.scene.create_entity(name);entity.mesh=std::move(mesh);entity.color=entity.mesh->suggested_color;
    entity.transform.position={0,1,0};g.selected=entity.id;g.name_target=0;g.status=L"Zaimportowano model 3D";
    LOG_INFO("import","Imported model: %s (%zu vertices, %zu triangles)",entity.name.c_str(),entity.mesh->vertices.size(),entity.mesh->triangles.size());
}

} // namespace editor
