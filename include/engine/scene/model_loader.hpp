#pragma once
#include "engine/export.hpp"
#include "engine/scene/scene.hpp"
#include <filesystem>
#include <memory>
#include <string>

namespace engine {
// Imports every 3D format supported by the bundled Assimp build. All meshes in
// the file are pre-transformed, combined, triangulated, centered and normalized
// to a two-unit box. Safety limits are applied before renderer-owned buffers are
// allocated.
[[nodiscard]] ENGINE_API std::shared_ptr<const MeshAsset> load_model(
    const std::filesystem::path& path,std::string& error);

// Backward-compatible entry point retained for callers introduced with OBJ v1.
[[nodiscard]] ENGINE_API std::shared_ptr<const MeshAsset> load_obj(
    const std::filesystem::path& path,std::string& error);
}
