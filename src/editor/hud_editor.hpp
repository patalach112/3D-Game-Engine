#pragma once
#include "engine/hud/document.hpp"
namespace editor {
void initialize_hud();
void draw_document_ui();
void hud_builtin(const std::string& name);
void hud_action(const std::string& name);
void editor_shortcuts();
bool save_hud();
}
