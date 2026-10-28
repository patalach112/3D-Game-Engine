#include "editor_app.hpp"

#include <fstream>

namespace editor {

void save_scene() {
    std::ofstream file("scene.txt");
    if (!file) {
        app().status = L"Nie udało się zapisać sceny";
        return;
    }
    file << "# Starter 3D Engine scene v3 (primitive type follows the name)\n";
    for (const auto& e : app().scene.entities()) {
        file << e.id << ' ' << e.name << ' ' << static_cast<int>(e.primitive) << ' '
             << e.transform.position.x << ' ' << e.transform.position.y << ' '
             << e.transform.position.z << ' ' << e.transform.rotation.x << ' ' << e.transform.rotation.y << ' '
             << e.transform.rotation.z << ' ' << e.transform.scale.x << ' ' << e.transform.scale.y << ' '
             << e.transform.scale.z << '\n';
    }
    app().status = L"Zapisano scenę do scene.txt";
}

} // namespace editor
