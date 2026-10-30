#pragma once
#include "engine/render/vulkan_renderer.hpp"
#include <algorithm>
#include <cmath>
namespace engine {
inline float grid_spacing(const EditorCamera& camera) {
    const double x=static_cast<double>(camera.position.x)-camera.target.x;
    const double y=static_cast<double>(camera.position.y)-camera.target.y;
    const double z=static_cast<double>(camera.position.z)-camera.target.z;
    // Quantization tolerance prevents floating-point orbit drift at thresholds.
    const double zoom=std::max(1.0,std::sqrt(x*x+y*y+z*z));
    return static_cast<float>(std::pow(2.0,std::floor(std::log2(zoom/8.0)+1e-5)));
}
}
