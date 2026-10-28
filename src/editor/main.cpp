#include "engine/scene/scene.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace {
constexpr int kPanelWidth = 226;
constexpr int kInspectorWidth = 252;
constexpr float kObjectSize = 1.0F;

Vector3 to_raylib(const engine::Vec3& value) {
    return {value.x, value.y, value.z};
}

engine::Entity& add_cube(engine::Scene& scene) {
    auto& cube = scene.create_entity("Cube " + std::to_string(scene.entity_count()));
    cube.transform.position = {0.0F, 0.5F, 0.0F};
    return cube;
}

void save_scene(const engine::Scene& scene) {
    std::ofstream file("scene.txt");
    if (!file) return;
    file << "# Starter 3D Engine scene v1\n";
    for (const auto& entity : scene.entities()) {
        file << entity.id << ' ' << entity.name << ' '
             << entity.transform.position.x << ' ' << entity.transform.position.y << ' '
             << entity.transform.position.z << ' ' << entity.transform.rotation.x << ' '
             << entity.transform.rotation.y << ' ' << entity.transform.rotation.z << ' '
             << entity.transform.scale.x << ' ' << entity.transform.scale.y << ' '
             << entity.transform.scale.z << '\n';
    }
}

bool button(Rectangle bounds, const char* label, bool enabled = true) {
    const Vector2 mouse = GetMousePosition();
    const bool hovered = CheckCollisionPointRec(mouse, bounds);
    const Color fill = !enabled ? Color{48, 53, 64, 255}
        : hovered ? Color{57, 119, 231, 255} : Color{43, 91, 177, 255};
    DrawRectangleRounded(bounds, 0.16F, 6, fill);
    DrawText(label, static_cast<int>(bounds.x + 12), static_cast<int>(bounds.y + 8), 16, RAYWHITE);
    return enabled && hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

void label(const char* text, int x, int y, int size = 15, Color color = LIGHTGRAY) {
    DrawText(text, x, y, size, color);
}

void draw_editor_ui(const engine::Scene& scene, const engine::Entity* selected,
                    bool& add_requested, bool& save_requested) {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const int right_x = width - kInspectorWidth;

    DrawRectangle(0, 0, width, 42, Color{24, 27, 34, 255});
    DrawText("STARTER 3D", 15, 12, 18, RAYWHITE);
    DrawText("SCENA", 146, 14, 13, Color{163, 177, 201, 255});
    DrawText("Silnik 0.1  |  Windows  |  C++", width - 315, 14, 13, Color{163, 177, 201, 255});

    DrawRectangle(0, 42, kPanelWidth, height - 42, Color{30, 34, 42, 255});
    DrawText("SCENA", 16, 58, 16, RAYWHITE);
    DrawLine(14, 83, kPanelWidth - 14, 83, Color{61, 67, 79, 255});
    if (button({14, 94, kPanelWidth - 28.0F, 34}, "+  Dodaj kostke")) add_requested = true;
    if (button({14, 136, kPanelWidth - 28.0F, 34}, "Zapisz scene")) save_requested = true;
    DrawText("OBIEKTY", 16, 186, 13, Color{143, 155, 176, 255});

    int row_y = 211;
    for (const auto& entity : scene.entities()) {
        const bool is_selected = selected && selected->id == entity.id;
        const Rectangle row{8, static_cast<float>(row_y), kPanelWidth - 16.0F, 28};
        if (is_selected) DrawRectangleRounded(row, 0.12F, 4, Color{49, 81, 132, 255});
        DrawCircle(23, row_y + 14, 5, is_selected ? SKYBLUE : Color{126, 145, 173, 255});
        DrawText(entity.name.c_str(), 37, row_y + 6, 14, is_selected ? RAYWHITE : LIGHTGRAY);
        row_y += 31;
    }

    DrawRectangle(right_x, 42, kInspectorWidth, height - 42, Color{30, 34, 42, 255});
    DrawText("INSPEKTOR", right_x + 16, 58, 16, RAYWHITE);
    DrawLine(right_x + 14, 83, width - 14, 83, Color{61, 67, 79, 255});
    if (!selected) {
        label("Zaznacz obiekt w scenie.", right_x + 16, 104);
        label("Dodaj kostke przyciskiem po lewej.", right_x + 16, 127, 13, Color{143, 155, 176, 255});
        return;
    }

    label(selected->name.c_str(), right_x + 16, 101, 17, SKYBLUE);
    label("TRANSFORMACJA", right_x + 16, 139, 13, Color{143, 155, 176, 255});
    const auto& t = selected->transform;
    label(TextFormat("Pozycja  X %5.2f  Y %5.2f  Z %5.2f", t.position.x, t.position.y, t.position.z), right_x + 16, 164, 13);
    label(TextFormat("Obrot     X %5.1f  Y %5.1f  Z %5.1f", t.rotation.x, t.rotation.y, t.rotation.z), right_x + 16, 188, 13);
    label(TextFormat("Skala     X %5.2f  Y %5.2f  Z %5.2f", t.scale.x, t.scale.y, t.scale.z), right_x + 16, 212, 13);
    DrawLine(right_x + 14, 243, width - 14, 243, Color{61, 67, 79, 255});
    label("SKROTY", right_x + 16, 260, 13, Color{143, 155, 176, 255});
    label("N  - dodaj kostke", right_x + 16, 285, 13);
    label("Delete  - usun obiekt", right_x + 16, 307, 13);
    label("PPM + ruch myszy  - obrot kamery", right_x + 16, 329, 12, Color{143, 155, 176, 255});
    label("Scroll  - przyblizenie", right_x + 16, 350, 12, Color{143, 155, 176, 255});
}

void draw_cube(const engine::Entity& entity, bool selected) {
    const auto& t = entity.transform;
    const Vector3 position = to_raylib(t.position);
    const Vector3 size{std::max(0.05F, t.scale.x), std::max(0.05F, t.scale.y), std::max(0.05F, t.scale.z)};
    const Color tint = selected ? Color{89, 166, 255, 255} : Color{154, 172, 197, 255};
    DrawCubeV(position, size, tint);
    DrawCubeWiresV(position, size, selected ? SKYBLUE : Color{55, 65, 82, 255});
}
} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 800, "Starter 3D Engine");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    engine::Scene scene;
    auto& starter_cube = add_cube(scene);
    const engine::EntityId starter_id = starter_cube.id;
    engine::EntityId selected_id = starter_id;

    float yaw = 0.75F;
    float pitch = 0.55F;
    float distance = 8.0F;
    Camera3D camera{};
    camera.target = {0.0F, 0.5F, 0.0F};
    camera.up = {0.0F, 1.0F, 0.0F};
    camera.fovy = 45.0F;
    camera.projection = CAMERA_PERSPECTIVE;

    bool add_requested = false;
    bool save_requested = false;
    while (!WindowShouldClose()) {
        const int width = GetScreenWidth();
        const int height = GetScreenHeight();
        const int viewport_left = kPanelWidth;
        const int viewport_right = width - kInspectorWidth;
        const bool over_viewport = GetMouseX() >= viewport_left && GetMouseX() < viewport_right
            && GetMouseY() >= 42 && GetMouseY() < height;

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && over_viewport) {
            const Vector2 delta = GetMouseDelta();
            yaw -= delta.x * 0.006F;
            pitch = std::clamp(pitch - delta.y * 0.006F, -1.35F, 1.35F);
        }
        distance = std::clamp(distance - GetMouseWheelMove() * 0.7F, 2.0F, 35.0F);
        camera.position = {
            camera.target.x + distance * std::cos(pitch) * std::sin(yaw),
            camera.target.y + distance * std::sin(pitch),
            camera.target.z + distance * std::cos(pitch) * std::cos(yaw)
        };

        if (IsKeyPressed(KEY_N)) add_requested = true;
        if (engine::Entity* selected = scene.find_entity(selected_id)) {
            constexpr float move_step = 0.15F;
            if (IsKeyDown(KEY_LEFT)) selected->transform.position.x -= move_step;
            if (IsKeyDown(KEY_RIGHT)) selected->transform.position.x += move_step;
            if (IsKeyDown(KEY_UP)) selected->transform.position.z -= move_step;
            if (IsKeyDown(KEY_DOWN)) selected->transform.position.z += move_step;
            if (IsKeyDown(KEY_PAGE_UP)) selected->transform.position.y += move_step;
            if (IsKeyDown(KEY_PAGE_DOWN)) selected->transform.position.y -= move_step;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseX() < kPanelWidth
            && GetMouseY() >= 211) {
            const int row = (GetMouseY() - 211) / 31;
            if (row >= 0 && static_cast<std::size_t>(row) < scene.entity_count()) {
                selected_id = scene.entities()[static_cast<std::size_t>(row)].id;
            }
        }
        if (IsKeyPressed(KEY_DELETE) && selected_id != 0) {
            (void)scene.remove_entity(selected_id);
            selected_id = scene.entity_count() == 0 ? 0 : scene.entities().front().id;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && over_viewport) {
            const Ray ray = GetMouseRay(GetMousePosition(), camera);
            float closest = 1.0e30F;
            engine::EntityId hit_id = 0;
            for (const auto& entity : scene.entities()) {
                const Vector3 p = to_raylib(entity.transform.position);
                const Vector3 half{std::max(0.05F, entity.transform.scale.x) * 0.5F,
                                   std::max(0.05F, entity.transform.scale.y) * 0.5F,
                                   std::max(0.05F, entity.transform.scale.z) * 0.5F};
                const RayCollision hit = GetRayCollisionBox(ray, {Vector3Subtract(p, half), Vector3Add(p, half)});
                if (hit.hit && hit.distance < closest) {
                    closest = hit.distance;
                    hit_id = entity.id;
                }
            }
            selected_id = hit_id;
        }

        if (add_requested) {
            selected_id = add_cube(scene).id;
            add_requested = false;
        }
        if (save_requested) {
            save_scene(scene);
            save_requested = false;
        }

        BeginDrawing();
        ClearBackground(Color{21, 24, 30, 255});
        BeginScissorMode(viewport_left, 42, std::max(1, viewport_right - viewport_left), std::max(1, height - 42));
        BeginMode3D(camera);
        DrawPlane({0.0F, -0.03F, 0.0F}, {200.0F, 200.0F}, Color{34, 39, 48, 255});
        DrawGrid(40, 1.0F);
        for (const auto& entity : scene.entities()) draw_cube(entity, entity.id == selected_id);
        EndMode3D();
        EndScissorMode();
        draw_editor_ui(scene, scene.find_entity(selected_id), add_requested, save_requested);
        DrawRectangle(0, height - 25, width, 25, Color{24, 27, 34, 255});
        DrawText("Gotowy  |  Zapis sceny: scene.txt", 12, height - 19, 12, Color{143, 155, 176, 255});
        DrawFPS(width - 75, height - 20);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
