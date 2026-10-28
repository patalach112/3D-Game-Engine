#include "engine/scene/scene.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <string>

namespace {
constexpr int kPanelWidth = 238;
constexpr int kInspectorWidth = 286;
constexpr int kTopBarHeight = 48;
constexpr int kSceneListTop = 215;
constexpr Color kCanvas{42, 42, 44, 255};
constexpr Color kPanel{29, 29, 30, 255};
constexpr Color kPanelRaised{36, 36, 37, 255};
constexpr Color kBorder{55, 55, 57, 255};
constexpr Color kTextMuted{165, 165, 168, 255};
constexpr Color kAccent{230, 112, 43, 255};
constexpr std::array<Vector3, 3> kAxes{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
constexpr std::array<Color, 3> kAxisColors{{{230, 73, 67, 255}, {95, 190, 105, 255}, {74, 142, 235, 255}}};

Font g_font{};
bool g_custom_font = false;

void text(const char* value, float x, float y, float size, Color color = RAYWHITE) {
    DrawTextEx(g_font, value, {x, y}, size, std::max(0.5F, size * 0.025F), color);
}

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

bool button(Rectangle bounds, const char* caption, bool primary = false) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    Color fill = primary ? Color{57, 57, 59, 255} : Color{43, 43, 44, 255};
    if (hovered) fill = primary ? Color{74, 74, 76, 255} : Color{55, 55, 57, 255};
    DrawRectangleRounded(bounds, 0.12F, 5, fill);
    DrawRectangleLinesEx(bounds, 1.0F, hovered ? kAccent : kBorder);
    text(caption, bounds.x + 12, bounds.y + 8, 15, RAYWHITE);
    return hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

void section_title(const char* value, int x, int y) {
    text(value, static_cast<float>(x), static_cast<float>(y), 13, kTextMuted);
}

void draw_editor_ui(const engine::Scene& scene, const engine::Entity* selected,
                    bool& add_requested, bool& save_requested) {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const int right_x = width - kInspectorWidth;

    DrawRectangle(0, 0, width, kTopBarHeight, Color{31, 31, 32, 255});
    DrawLine(0, kTopBarHeight - 1, width, kTopBarHeight - 1, kBorder);
    DrawRectangle(13, 12, 24, 24, kAccent);
    text("S", 19, 14, 17, RAYWHITE);
    text("STARTER", 47, 14, 16, RAYWHITE);
    text("SCENA", 143, 17, 13, kTextMuted);
    DrawLine(143, 43, 189, 43, kAccent);
    text("Silnik 0.1", static_cast<float>(width - 214), 17, 13, kTextMuted);
    DrawCircle(width - 177, 23, 2, kBorder);
    text("Windows", static_cast<float>(width - 162), 17, 13, kTextMuted);
    DrawCircle(width - 98, 23, 2, kBorder);
    text("C++", static_cast<float>(width - 83), 17, 13, kTextMuted);

    DrawRectangle(0, kTopBarHeight, kPanelWidth, height - kTopBarHeight, kPanel);
    DrawRectangle(right_x, kTopBarHeight, kInspectorWidth, height - kTopBarHeight, kPanel);
    DrawLine(kPanelWidth - 1, kTopBarHeight, kPanelWidth - 1, height, kBorder);
    DrawLine(right_x, kTopBarHeight, right_x, height, kBorder);

    text("Hierarchia", 17, 65, 17, RAYWHITE);
    if (button({kPanelWidth - 69.0F, 59, 28, 28}, "+", true)) add_requested = true;
    if (button({kPanelWidth - 36.0F, 59, 25, 28}, "...")) { }
    DrawLine(12, 94, kPanelWidth - 12, 94, kBorder);
    if (button({12, 105, kPanelWidth - 24.0F, 34}, "+  Dodaj kostkę", true)) add_requested = true;
    if (button({12, 146, kPanelWidth - 24.0F, 34}, "Zapisz scenę")) save_requested = true;
    section_title("OBIEKTY SCENY", 16, 193);

    int row_y = kSceneListTop;
    for (const auto& entity : scene.entities()) {
        const bool is_selected = selected && selected->id == entity.id;
        const Rectangle row{7, static_cast<float>(row_y), kPanelWidth - 14.0F, 31};
        if (is_selected) {
            DrawRectangleRec(row, Color{49, 49, 50, 255});
            DrawRectangle(static_cast<int>(row.x), static_cast<int>(row.y), 3, static_cast<int>(row.height), kAccent);
        }
        DrawRectangle(18, row_y + 10, 10, 10,
                      is_selected ? Color{190, 190, 192, 255} : kTextMuted);
        text(entity.name.c_str(), 39, static_cast<float>(row_y + 7), 14,
             is_selected ? RAYWHITE : kTextMuted);
        row_y += 33;
    }

    text("Inspektor", static_cast<float>(right_x + 16), 65, 17, RAYWHITE);
    DrawLine(right_x + 12, 94, width - 12, 94, kBorder);
    if (!selected) {
        text("Brak zaznaczenia", static_cast<float>(right_x + 16), 111, 14, kTextMuted);
        text("Kliknij obiekt w widoku sceny.", static_cast<float>(right_x + 16), 136, 13, kTextMuted);
        return;
    }

    text(selected->name.c_str(), static_cast<float>(right_x + 16), 108, 16, kAccent);
    const auto& t = selected->transform;
    DrawRectangleRec({static_cast<float>(right_x + 12), 139, kInspectorWidth - 24.0F, 27}, kPanelRaised);
    section_title("TRANSFORMACJA", right_x + 20, 146);
    text(TextFormat("Pozycja    X %5.2f   Y %5.2f   Z %5.2f", t.position.x, t.position.y, t.position.z),
         static_cast<float>(right_x + 18), 178, 12, RAYWHITE);
    text(TextFormat("Obrót      X %5.1f   Y %5.1f   Z %5.1f", t.rotation.x, t.rotation.y, t.rotation.z),
         static_cast<float>(right_x + 18), 201, 12, RAYWHITE);
    text(TextFormat("Skala      X %5.2f   Y %5.2f   Z %5.2f", t.scale.x, t.scale.y, t.scale.z),
         static_cast<float>(right_x + 18), 224, 12, RAYWHITE);

    DrawRectangleRec({static_cast<float>(right_x + 12), 257, kInspectorWidth - 24.0F, 27}, kPanelRaised);
    section_title("NARZĘDZIA", right_x + 20, 264);
    text("Przesuń kulki osi na obiekcie.", static_cast<float>(right_x + 18), 297, 12, kTextMuted);
    text("PPM + przeciąganie: obrót kamery", static_cast<float>(right_x + 18), 319, 12, kTextMuted);
    text("Kółko myszy: przybliżenie", static_cast<float>(right_x + 18), 341, 12, kTextMuted);
    text("Delete: usuń zaznaczony obiekt", static_cast<float>(right_x + 18), 363, 12, kTextMuted);
}

void draw_cube(const engine::Entity& entity, bool selected) {
    const auto& t = entity.transform;
    const Vector3 position = to_raylib(t.position);
    const Vector3 size{std::max(0.05F, t.scale.x), std::max(0.05F, t.scale.y), std::max(0.05F, t.scale.z)};
    DrawCubeV(position, size, selected ? Color{185, 185, 188, 255} : Color{139, 142, 146, 255});
    DrawCubeWiresV(position, size, selected ? Color{245, 245, 245, 255} : Color{77, 78, 80, 255});
}

float gizmo_length(float camera_distance) {
    return std::clamp(camera_distance * 0.13F, 0.85F, 2.3F);
}

void draw_move_gizmo(const engine::Entity& entity, float camera_distance) {
    const Vector3 center = to_raylib(entity.transform.position);
    const float length = gizmo_length(camera_distance);
    const float radius = std::clamp(length * 0.10F, 0.10F, 0.21F);
    for (std::size_t axis = 0; axis < kAxes.size(); ++axis) {
        for (float sign : {-1.0F, 1.0F}) {
            const Vector3 direction = Vector3Scale(kAxes[axis], sign);
            const Vector3 handle = Vector3Add(center, Vector3Scale(direction, length));
            DrawLine3D(center, handle, kAxisColors[axis]);
            DrawSphere(handle, radius, kAxisColors[axis]);
        }
    }
}

void draw_editor_grid() {
    constexpr int extent = 24;
    for (int i = -extent; i <= extent; ++i) {
        const bool major = (i % 5) == 0;
        const Color color = major ? Color{82, 82, 84, 255} : Color{59, 59, 61, 255};
        const float coordinate = static_cast<float>(i);
        DrawLine3D({-static_cast<float>(extent), -0.025F, coordinate},
                   {static_cast<float>(extent), -0.025F, coordinate}, color);
        DrawLine3D({coordinate, -0.025F, -static_cast<float>(extent)},
                   {coordinate, -0.025F, static_cast<float>(extent)}, color);
    }
}

bool ray_plane_hit(const Ray& ray, Vector3 point, Vector3 normal, Vector3& result) {
    const float denominator = Vector3DotProduct(ray.direction, normal);
    if (std::abs(denominator) < 1.0e-5F) return false;
    const float distance = Vector3DotProduct(Vector3Subtract(point, ray.position), normal) / denominator;
    if (distance < 0.0F) return false;
    result = Vector3Add(ray.position, Vector3Scale(ray.direction, distance));
    return true;
}

bool begin_gizmo_drag(const Ray& ray, const engine::Entity& entity, float camera_distance,
                      int& drag_axis, Vector3& drag_direction, Vector3& plane_normal,
                      Vector3& initial_hit, engine::Vec3& initial_position, const Camera3D& camera) {
    const Vector3 center = to_raylib(entity.transform.position);
    const float length = gizmo_length(camera_distance);
    const float radius = std::clamp(length * 0.14F, 0.14F, 0.29F);
    float closest_distance = 1.0e30F;
    int picked_axis = -1;
    Vector3 picked_direction{};

    for (int axis = 0; axis < static_cast<int>(kAxes.size()); ++axis) {
        for (float sign : {-1.0F, 1.0F}) {
            const Vector3 direction = Vector3Scale(kAxes[static_cast<std::size_t>(axis)], sign);
            const Vector3 handle = Vector3Add(center, Vector3Scale(direction, length));
            const RayCollision hit = GetRayCollisionSphere(ray, handle, radius * 1.35F);
            if (hit.hit && hit.distance < closest_distance) {
                closest_distance = hit.distance;
                picked_axis = axis;
                picked_direction = direction;
            }
        }
    }
    if (picked_axis < 0) return false;

    const Vector3 view_direction = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 normal = Vector3Subtract(view_direction,
        Vector3Scale(picked_direction, Vector3DotProduct(view_direction, picked_direction)));
    if (Vector3Length(normal) < 1.0e-4F) {
        const Vector3 fallback = std::abs(picked_direction.y) < 0.9F
            ? Vector3{0.0F, 1.0F, 0.0F} : Vector3{1.0F, 0.0F, 0.0F};
        normal = Vector3CrossProduct(picked_direction, fallback);
    }
    plane_normal = Vector3Normalize(normal);
    if (!ray_plane_hit(ray, center, plane_normal, initial_hit)) return false;
    drag_axis = picked_axis;
    drag_direction = picked_direction;
    initial_position = entity.transform.position;
    return true;
}
} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 800, "Starter 3D Engine");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    const std::string font_path = std::string(GetApplicationDirectory()) + "assets/fonts/Inter.ttf";
    g_font = LoadFontEx(font_path.c_str(), 32, nullptr, 0);
    g_custom_font = IsFontValid(g_font);
    if (!g_custom_font) g_font = GetFontDefault();
    else SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);

    engine::Scene scene;
    auto& starter_cube = add_cube(scene);
    engine::EntityId selected_id = starter_cube.id;

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
    int drag_axis = -1;
    Vector3 drag_direction{};
    Vector3 drag_plane_normal{};
    Vector3 drag_start_hit{};
    engine::Vec3 drag_start_position{};

    while (!WindowShouldClose()) {
        const int width = GetScreenWidth();
        const int height = GetScreenHeight();
        const int viewport_left = kPanelWidth;
        const int viewport_right = width - kInspectorWidth;
        const bool over_viewport = GetMouseX() >= viewport_left && GetMouseX() < viewport_right
            && GetMouseY() >= kTopBarHeight && GetMouseY() < height - 25;

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && over_viewport) {
            const Vector2 delta = GetMouseDelta();
            yaw -= delta.x * 0.006F;
            // Mouse-up now moves the camera's view upward, matching common editor controls.
            pitch = std::clamp(pitch + delta.y * 0.006F, -1.35F, 1.35F);
        }
        distance = std::clamp(distance - GetMouseWheelMove() * 0.7F, 2.0F, 35.0F);
        camera.position = {
            camera.target.x + distance * std::cos(pitch) * std::sin(yaw),
            camera.target.y + distance * std::sin(pitch),
            camera.target.z + distance * std::cos(pitch) * std::cos(yaw)
        };

        if (IsKeyPressed(KEY_N)) add_requested = true;
        if (IsKeyPressed(KEY_DELETE) && selected_id != 0 && drag_axis < 0) {
            (void)scene.remove_entity(selected_id);
            selected_id = scene.entity_count() == 0 ? 0 : scene.entities().front().id;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) drag_axis = -1;

        if (drag_axis >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (const engine::Entity* selected = scene.find_entity(selected_id)) {
                const Ray ray = GetMouseRay(GetMousePosition(), camera);
                Vector3 current_hit{};
                if (ray_plane_hit(ray, to_raylib(selected->transform.position), drag_plane_normal, current_hit)) {
                    const float amount = Vector3DotProduct(Vector3Subtract(current_hit, drag_start_hit), drag_direction);
                    if (engine::Entity* mutable_selected = scene.find_entity(selected_id)) {
                        mutable_selected->transform.position = {
                            drag_start_position.x + drag_direction.x * amount,
                            drag_start_position.y + drag_direction.y * amount,
                            drag_start_position.z + drag_direction.z * amount
                        };
                    }
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseX() < kPanelWidth
            && GetMouseY() >= kSceneListTop) {
            const int row = (GetMouseY() - kSceneListTop) / 33;
            if (row >= 0 && static_cast<std::size_t>(row) < scene.entity_count())
                selected_id = scene.entities()[static_cast<std::size_t>(row)].id;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && over_viewport) {
            const Ray ray = GetMouseRay(GetMousePosition(), camera);
            bool started_drag = false;
            if (const engine::Entity* selected = scene.find_entity(selected_id)) {
                started_drag = begin_gizmo_drag(ray, *selected, distance, drag_axis, drag_direction,
                    drag_plane_normal, drag_start_hit, drag_start_position, camera);
            }
            if (!started_drag) {
                float closest = 1.0e30F;
                engine::EntityId hit_id = 0;
                for (const auto& entity : scene.entities()) {
                    const Vector3 p = to_raylib(entity.transform.position);
                    const Vector3 half{std::max(0.05F, entity.transform.scale.x) * 0.5F,
                                       std::max(0.05F, entity.transform.scale.y) * 0.5F,
                                       std::max(0.05F, entity.transform.scale.z) * 0.5F};
                    const RayCollision hit = GetRayCollisionBox(ray,
                        {Vector3Subtract(p, half), Vector3Add(p, half)});
                    if (hit.hit && hit.distance < closest) {
                        closest = hit.distance;
                        hit_id = entity.id;
                    }
                }
                selected_id = hit_id;
            }
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
        ClearBackground(kCanvas);
        BeginScissorMode(viewport_left, kTopBarHeight,
            std::max(1, viewport_right - viewport_left), std::max(1, height - kTopBarHeight - 25));
        BeginMode3D(camera);
        DrawPlane({0.0F, -0.03F, 0.0F}, {200.0F, 200.0F}, Color{47, 47, 49, 255});
        draw_editor_grid();
        for (const auto& entity : scene.entities()) draw_cube(entity, entity.id == selected_id);
        if (const engine::Entity* selected = scene.find_entity(selected_id)) draw_move_gizmo(*selected, distance);
        EndMode3D();
        EndScissorMode();

        draw_editor_ui(scene, scene.find_entity(selected_id), add_requested, save_requested);
        DrawRectangle(0, height - 25, width, 25, Color{31, 31, 32, 255});
        DrawLine(0, height - 25, width, height - 25, kBorder);
        text(TextFormat("Gotowe  |  Zapis sceny: scene.txt  |  Obiektów: %i", static_cast<int>(scene.entity_count())),
             12, static_cast<float>(height - 19), 11, kTextMuted);
        DrawFPS(width - 70, height - 20);
        EndDrawing();
    }

    if (g_custom_font) UnloadFont(g_font);
    CloseWindow();
    return 0;
}
