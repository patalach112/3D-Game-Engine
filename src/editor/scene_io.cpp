#include "editor_app.hpp"
#include "engine/log.hpp"

#include <fstream>
#include <iomanip>

namespace editor {

void save_scene() {
    std::ofstream file("scene.txt");
    if (!file) {
        app().status = L"Nie udało się zapisać sceny";
        LOG_ERROR("editor","Nie udało się otworzyć scene.txt");
        return;
    }
    file << "# Starter 3D Engine scene v4 (UTF-8 quoted name, then primitive type)\n";
    for (const auto& e : app().scene.entities()) {
        file << e.id << ' ' << std::quoted(e.name) << ' ' << static_cast<int>(e.primitive) << ' '
             << e.transform.position.x << ' ' << e.transform.position.y << ' '
             << e.transform.position.z << ' ' << e.transform.rotation.x << ' ' << e.transform.rotation.y << ' '
             << e.transform.rotation.z << ' ' << e.transform.scale.x << ' ' << e.transform.scale.y << ' '
             << e.transform.scale.z << '\n';
    }
    file.flush();
    if (file) LOG_INFO("editor","Zapisano scenę do scene.txt");
    else LOG_ERROR("editor","Błąd zapisu scene.txt");
    app().status = file?L"Zapisano scenę do scene.txt":L"Błąd zapisu sceny";
}

} // namespace editor
