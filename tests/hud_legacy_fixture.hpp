#pragma once
// Genuine pre-colour fixtures. Changing the version number on a v3 document
// does not turn its palette-bearing records into legacy v1/v2 records.
#include "engine/hud/document.hpp"
#include <fstream>
#include <iomanip>
inline void write_legacy_hud(const std::filesystem::path& path,const engine::hud::Document& doc,int version) {
    std::ofstream f(path,std::ios::binary);f<<std::setprecision(9)<<"STUDIO_HUD "<<version<<'\n'
        <<doc.next_id<<' '<<doc.window_minimum.x<<' '<<doc.window_minimum.y<<' '<<doc.default_rounding<<' '<<doc.nodes.size()<<'\n';
    for(const auto& n:doc.nodes)f<<n.id<<' '<<n.parent<<' '<<static_cast<int>(n.kind)<<' '<<static_cast<int>(n.dock)<<' '
        <<n.visible<<' '<<n.enabled<<' '<<n.rounded<<' '<<n.shared_rounding<<' '<<n.rounding<<' '
        <<n.minimum.x<<' '<<n.minimum.y<<' '<<n.gap.x<<' '<<n.gap.y<<' '<<n.padding.x<<' '<<n.padding.y<<' '<<n.button_minimum.x<<' '<<n.button_minimum.y<<' '
        <<n.rect.x<<' '<<n.rect.y<<' '<<n.rect.w<<' '<<n.rect.h<<' '
        <<std::quoted(n.text)<<' '<<std::quoted(n.action)<<' '<<std::quoted(n.builtin)<<' '<<std::quoted(n.value)<<'\n';
    f<<std::quoted(doc.docking)<<'\n';
    if(version>=2) {f<<doc.bindings.size()<<'\n';for(const auto& b:doc.bindings)f<<std::quoted(b.action)<<' '<<b.key<<' '<<b.modifiers<<'\n';}
}
