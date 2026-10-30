#pragma once
#include "engine/export.hpp"
#include "engine/hud/bindings.hpp"
#include <cstdint>
#include <array>
#include <span>
#include <unordered_map>
#include <filesystem>
#include <string>
#include <vector>

namespace engine::hud {
using Id=std::uint32_t;
enum class Kind { Bar, Window, Panel, Button, Input, Text, List, Footer };
enum class Dock { Floating, Left, Center, Right, Bottom };
struct Size { float x{},y{}; };
struct Rect { float x{},y{},w{1},h{1}; }; // Relative to the design canvas, not desktop pixels.
enum class ColorRole { Background, Panel, Text, Border, Control, Hovered, Active, Accent, Count };
using Color=std::array<float,4>; // linear UI RGBA components in [0,1], including alpha.
struct Palette {
    std::array<Color,static_cast<std::size_t>(ColorRole::Count)> colors{{
        {.065F,.075F,.095F,1}, {.14F,.15F,.17F,1}, {.95F,.96F,.98F,1}, {.30F,.33F,.38F,1},
        {.15F,.18F,.23F,1}, {.21F,.31F,.40F,1}, {.12F,.40F,.49F,1}, {.31F,.85F,.79F,1}
    }};
    Color& operator[](ColorRole role) {return colors[static_cast<std::size_t>(role)];}
    const Color& operator[](ColorRole role) const {return colors[static_cast<std::size_t>(role)];}
    bool operator==(const Palette&) const = default;
};
struct Node {
    Id id{},parent{};
    Kind kind{Kind::Panel};
    std::string text, action, builtin, value;
    bool visible{true}, enabled{true}, rounded{true}, shared_rounding{true};
    float rounding{10};
    Size minimum{120,80}, gap{8,8}, padding{10,10}, button_minimum{80,26};
    Rect rect;
    Dock dock{Dock::Floating};
    bool custom_colors{};
    Palette palette;
};
struct Document {
    std::vector<Node> nodes;
    Id next_id{1};
    Size window_minimum{900,600};
    float default_rounding{10};
    std::string docking;
    bool custom_colors{true};
    Palette palette;
    std::vector<Binding> bindings{basic_bindings()};
    ENGINE_API Node* find(Id id);
    ENGINE_API const Node* find(Id id) const;
    ENGINE_API Id add(Kind kind,Id parent,const std::string& text);
    ENGINE_API void erase(Id id);
    ENGINE_API bool validate(std::string& error) const;
};
// Public node fields remain editable: compare structural keys, not document
// identity or a revision counter that could miss direct edits. Children retain
// authoring order; sparse IDs never cause ID-sized allocations.
class HierarchyIndex {
public:
    ENGINE_API bool update(const Document& document); // true only on rebuild
    ENGINE_API std::span<const std::size_t> children(Id parent) const noexcept;
    std::uint64_t builds{},hits{};
private:
    struct Key {Id id,parent;bool operator==(const Key&) const = default;};
    std::vector<Key> keys_;
    std::unordered_map<Id,std::vector<std::size_t>> children_;
    bool valid_{};
};
ENGINE_API Document defaults();
ENGINE_API bool load(const std::filesystem::path& path,Document& out,std::string& error);
ENGINE_API bool save(const std::filesystem::path& path,const Document& doc,std::string& error);
ENGINE_API bool accepts(Kind parent,Kind child);
ENGINE_API std::string window_name(const Node& node);
}
