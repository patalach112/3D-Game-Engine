#pragma once
#include "engine/export.hpp"
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace engine::hud {
// Stable, backend-independent codes. ASCII letters/digits retain their values.
enum class Key : int {
    None=0, Tab=256, Left, Right, Up, Down, PageUp, PageDown, Home, End,
    Insert, Delete, Backspace, Space, Enter, Escape, Minus, Equal, Comma, Period,
    Slash, Semicolon, Apostrophe, LeftBracket, RightBracket, Backslash, Grave,
    KeypadAdd, KeypadSubtract, KeypadEnter,
    F1=300, F24=323, MouseLeft=400, MouseRight, MouseMiddle, MouseX1, MouseX2
};
enum Modifier : unsigned { Ctrl=1,Shift=2,Alt=4,Super=8 };
struct Binding {
    std::string action;
    int key{};
    unsigned modifiers{};
    bool operator==(const Binding&) const = default;
};
struct Action { const char* id; const char* label; };
ENGINE_API std::span<const Action> actions();
ENGINE_API bool known_action(const std::string& id);
ENGINE_API bool valid_key(int key);
ENGINE_API std::string key_label(int key);
ENGINE_API std::string binding_label(const Binding& binding);
ENGINE_API std::vector<Binding> basic_bindings();
ENGINE_API bool validate_bindings(const std::vector<Binding>& bindings,std::string& error);
ENGINE_API std::uint64_t bindings_fingerprint(const std::vector<Binding>& bindings);
ENGINE_API bool load_bindings(const std::filesystem::path& path,std::uint64_t defaults_hash,std::vector<Binding>& out,std::string& error);
ENGINE_API bool save_bindings(const std::filesystem::path& path,std::uint64_t defaults_hash,const std::vector<Binding>& bindings,std::string& error);
}
