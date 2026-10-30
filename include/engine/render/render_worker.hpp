#pragma once
#include "engine/export.hpp"
#include <functional>
#include <memory>
namespace engine {
// Single-owner GPU executor, one frame in flight on the CPU. Frontend hands off
// renderer/scene/ImGui ownership until busy()==false (acquire) or wait() returns.
class ENGINE_API RenderWorker {
public:
    explicit RenderWorker(std::function<void()> completion={});
    ~RenderWorker();
    RenderWorker(const RenderWorker&)=delete;
    RenderWorker& operator=(const RenderWorker&)=delete;
    void submit(std::function<void()> frame);
    bool busy() const noexcept;
    void wait(); // rethrows worker failures on the frontend thread
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
