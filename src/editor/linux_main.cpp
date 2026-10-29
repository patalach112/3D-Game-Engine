#include "engine/render/vulkan_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int kPreviewWidth = 72;
constexpr int kPreviewHeight = 48;

void save_scene(const engine::Scene& scene) {
    std::ofstream file("scene.txt");
    file << "# Starter 3D Engine scene v3 (primitive type follows the name)\n";
    for (const auto& entity : scene.entities()) {
        const auto& t = entity.transform;
        file << entity.id << ' ' << entity.name << ' ' << static_cast<int>(entity.primitive) << ' '
             << t.position.x << ' ' << t.position.y << ' ' << t.position.z << ' '
             << t.rotation.x << ' ' << t.rotation.y << ' ' << t.rotation.z << ' '
             << t.scale.x << ' ' << t.scale.y << ' ' << t.scale.z << '\n';
    }
}

void write_preview(const engine::VulkanRenderer& renderer) {
    std::ofstream file("scene_preview.ppm", std::ios::binary);
    if (!file) return;
    file << "P6\n" << renderer.width() << ' ' << renderer.height() << "\n255\n";
    for (const std::uint32_t pixel : renderer.pixels()) {
        const std::array<unsigned char, 3> rgb{
            static_cast<unsigned char>((pixel >> 16U) & 0xffU),
            static_cast<unsigned char>((pixel >> 8U) & 0xffU),
            static_cast<unsigned char>(pixel & 0xffU)
        };
        file.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    }
}

void print_terminal_preview(const engine::VulkanRenderer& renderer) {
    const auto& pixels = renderer.pixels();
    const int width = renderer.width();
    const int height = renderer.height();
    std::cout << "\033[2J\033[H";
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; ++x) {
            const std::uint32_t top = pixels[static_cast<std::size_t>(y) * width + x];
            const std::uint32_t bottom = y + 1 < height
                ? pixels[static_cast<std::size_t>(y + 1) * width + x] : top;
            std::cout << "\033[38;2;" << ((top >> 16U) & 255U) << ';'
                      << ((top >> 8U) & 255U) << ';' << (top & 255U)
                      << "m\033[48;2;" << ((bottom >> 16U) & 255U) << ';'
                      << ((bottom >> 8U) & 255U) << ';' << (bottom & 255U)
                      << "m▀";
        }
        std::cout << "\033[0m\n";
    }
}

void render(engine::Scene& scene, engine::VulkanRenderer& renderer,
            const engine::EditorCamera& camera, engine::EntityId selected) {
    renderer.resize(kPreviewWidth, kPreviewHeight);
    renderer.render(scene, camera, selected);
    write_preview(renderer);
    print_terminal_preview(renderer);
}

void print_help() {
    std::cout << "\nStarter 3D Engine — Linux renderer preview\n"
              << "Komendy: add | select ID | move X Y Z | delete | save | render | help | quit\n"
              << "Po renderowaniu aktualny podgląd zapisuje się także do scene_preview.ppm.\n";
}
} // namespace

int run_editor() {
    engine::Scene scene;
    auto& first_cube = scene.create_entity("Cube 0");
    first_cube.transform.position = {0.0F, 0.5F, 0.0F};
    engine::EntityId selected = first_cube.id;
    engine::EditorCamera camera{{5.0F, 4.0F, 7.0F}, {0.0F, 0.5F, 0.0F}, 45.0F};
    engine::VulkanRenderer renderer;

    print_help();
    render(scene, renderer, camera, selected);
    std::string line;
    while (std::cout << "\nengine> " && std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command.empty()) continue;
        if (command == "quit" || command == "q") break;
        if (command == "help") { print_help(); continue; }
        if (command == "add") {
            auto& cube = scene.create_entity("Cube " + std::to_string(scene.entity_count()));
            cube.transform.position = {static_cast<float>(scene.entity_count() - 1) * 1.2F, 0.5F, 0.0F};
            selected = cube.id;
        } else if (command == "select") {
            engine::EntityId id{};
            if (!(input >> id) || !scene.find_entity(id)) std::cout << "Nie ma obiektu o takim ID.\n";
            else selected = id;
        } else if (command == "move") {
            engine::Vec3 position{};
            if (input >> position.x >> position.y >> position.z) {
                if (auto* entity = scene.find_entity(selected)) entity->transform.position = position;
            } else std::cout << "Użycie: move X Y Z\n";
        } else if (command == "delete") {
            if (selected != 0) (void)scene.remove_entity(selected);
            selected = scene.entity_count() ? scene.entities().front().id : 0;
        } else if (command == "save") {
            save_scene(scene);
            std::cout << "Zapisano scene.txt\n";
            continue;
        } else if (command != "render") {
            std::cout << "Nieznana komenda. Wpisz help.\n";
            continue;
        }
        render(scene, renderer, camera, selected);
    }
    std::cout << "Do zobaczenia.\n";
    return 0;
}

int main() {
    try { return run_editor(); }
    catch (const std::exception& error) {
        std::cerr << "Vulkan renderer error: " << error.what() << '\n';
        return 1;
    }
}
