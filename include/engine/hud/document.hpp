#pragma once
#include "engine/export.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine::hud {
using Id=std::uint32_t;
enum class Kind { Bar, Window, Panel, Button, Input, Text, List };
enum class Dock { Floating, Left, Center, Right, Bottom };
struct Size { float x{},y{}; };
struct Rect { float x{},y{},w{1},h{1}; }; // Relative to the design canvas, not desktop pixels.
struct Node {
    Id id{},parent{};
    Kind kind{Kind::Panel};
    std::string text, action, builtin, value;
    bool visible{true}, enabled{true}, rounded{true}, shared_rounding{true};
    float rounding{10};
    Size minimum{120,80}, gap{8,8}, padding{10,10}, button_minimum{80,26};
    Rect rect;
    Dock dock{Dock::Floating};
};
struct Document {
    std::vector<Node> nodes;
    Id next_id{1};
    Size window_minimum{900,600};
    float default_rounding{10};
    std::string docking;
    ENGINE_API Node* find(Id id);
    ENGINE_API const Node* find(Id id) const;
    ENGINE_API Id add(Kind kind,Id parent,const std::string& text);
    ENGINE_API void erase(Id id);
    ENGINE_API bool validate(std::string& error) const;
};
ENGINE_API Document defaults();
ENGINE_API bool load(const std::filesystem::path& path,Document& out,std::string& error);
ENGINE_API bool save(const std::filesystem::path& path,const Document& doc,std::string& error);
ENGINE_API bool accepts(Kind parent,Kind child);
ENGINE_API std::string window_name(const Node& node);
}
