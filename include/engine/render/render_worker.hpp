#pragma once
#include "engine/export.hpp"
#include <functional>
#include <memory>
namespace engine {
// Single-owner GPU executor, one frame in flight on the CPU. Frontend hands off
// renderer/scene/ImGui ownership until busy()==false (acquire) or wait() returns.
class RenderWorker {
public:
    ENGINE_API explicit RenderWorker(std::function<void()> completion={});
    ENGINE_API ~RenderWorker();
    RenderWorker(const RenderWorker&)=delete;
    RenderWorker& operator=(const RenderWorker&)=delete;
    ENGINE_API void submit(std::function<void()> frame);
    ENGINE_API bool busy() const noexcept;
    ENGINE_API void wait(); // rethrows worker failures on the frontend thread
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
