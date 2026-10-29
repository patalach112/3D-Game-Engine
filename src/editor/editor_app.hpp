#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include "engine/render/vulkan_renderer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace editor {

// Wysokość paska nagłówka i stopki. Okno widoku sceny leży między nimi.
constexpr int kHeaderHeight = 42;
constexpr int kFooterHeight = 27;

struct App {
    engine::Scene scene;
    engine::VulkanRenderer renderer;
    HWND viewport_window{}; // Vulkan child surface; panels remain GDI.
    RECT viewport_bounds{};
    bool renderer_failed{};
    engine::EditorCamera camera;
    engine::EntityId selected{};

    int width{1280};
    int height{800};
    int left_width{210};
    int right_width{270};
    bool layout_initialized{};
    bool resizing_left_panel{};
    bool resizing_right_panel{};

    float yaw{0.75F};
    float pitch{0.42F};
    float camera_distance{8.0F};
    bool orbiting{};
    POINT last_mouse{};

    int drag_axis{-1};
    float drag_sign{1.0F};
    engine::Vec3 drag_start_position{};
    engine::Vec3 drag_start_hit{};
    engine::Vec3 drag_plane_normal{};

    std::wstring status{L"Gotowe"};

    // Aktualnie edytowane pole inspektora: 0..2 pozycja, 3..5 obrót, 6..8 skala.
    int active_field{-1};
    std::wstring field_input;
    bool replace_field_on_char{true};

    // Zmiana kamery, sceny, zaznaczenia lub rozmiaru widoku wymusza przebudowę.
    bool scene_dirty{true};
    bool viewport_dirty{true}; // Expose/restore needs present; UI-only edits do not.

    HFONT font{};
    HFONT bold_font{};
    int font_height{};

    // Renderowana offline warstwa interfejsu. Rebuild, gdy zmienią się rozmiary
    // okna, szerokości paneli, lista obiektów, zaznaczenie lub edytowane pole.
    HDC ui_dc{};
    HBITMAP ui_bitmap{};
    HBITMAP ui_previous{};
    int ui_width{};
    int ui_height{};
    std::wstring ui_signature;
};

// Wspólny stan edytora, dostępny z każdego modułu.
App& app();

// Małe pomocniki wektorowe, pracujące na typach z namespace engine.
engine::Vec3 add(engine::Vec3 a, engine::Vec3 b);
engine::Vec3 sub(engine::Vec3 a, engine::Vec3 b);
engine::Vec3 mul(engine::Vec3 a, float n);
float dot(engine::Vec3 a, engine::Vec3 b);
engine::Vec3 cross(engine::Vec3 a, engine::Vec3 b);
float magnitude(engine::Vec3 a);
engine::Vec3 normalized(engine::Vec3 a);

// Kamery, layout i geometria paneli.
void update_camera();
void update_layout();
RECT view_rect();
RECT inspector_field_rect(int field);

// Wartości inspektora i edycja pól tekstowych.
float* inspector_value(int field);
int field_at(int x, int y);
std::wstring number_text(float value, int precision = 2);
void begin_field_edit(int field);
void commit_field_edit();

// Obsługa GDI: tekst, przyciski, pola, ikony.
void set_font(HDC dc, bool bold = false);
void fill(HDC dc, RECT rect, COLORREF color);
void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color);
void draw_text(HDC dc, const std::wstring& value, int x, int y, COLORREF color,
               int max_width = 0, bool bold = false);
void draw_button(HDC dc, RECT rect, const wchar_t* caption, bool primary = false);
void draw_input(HDC dc, int field);
void draw_entity_icon(HDC dc, const engine::Entity& entity, int x, int y, COLORREF color);
void draw_ui(HDC dc);

// Buduje (lub odświeża) zapisany offline interfejs. source_dc powinien być DC
// okna, z którego tworzone są bitmapy kompatybilne.
void ensure_ui_cache(HDC source_dc);
// Przy zamykaniu aplikacji: usuwa zapasową warstwę interfejsu.
void release_ui_cache();
void release_editor_resources();

// Rasteryzacja widoku sceny i prezentacja go bezpośrednio w oknie.
void render_frame(HWND hwnd);

// Scena i rzutowania strzałki na obiekt.
void save_scene();
void add_primitive(engine::PrimitiveType type);
bool ray_plane(engine::Ray3 ray, engine::Vec3 point, engine::Vec3 normal, engine::Vec3& hit);
bool ray_box(engine::Ray3 ray, const engine::Entity& entity, float& distance);
bool hit_gizmo(int sx, int sy, int view_w, int view_h, engine::Entity& entity);

// Procedurowa procedura okna.
LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);

} // namespace editor
