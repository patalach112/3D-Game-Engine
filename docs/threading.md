# Frame loop, camera and input threading

## No software FPS cap

The Windows editor continuously prepares a new frame as soon as its render worker
finishes the preceding CPU job. There is no `WM_TIMER`, 16 ms sleep or fixed FPS
target. It pumps messages while GPU work is in progress and sleeps on OS events
when minimized; minimized time is not included in the next frame delta.

The swapchain requests **IMMEDIATE** presentation when available (tearing is
possible), otherwise MAILBOX, finally the mandatory FIFO fallback. The driver,
window compositor, GPU throughput and FIFO-only surfaces can still constrain
presentation. “Uncapped” does not mean bypassing hardware/driver synchronization.
The existing FPS counter measures actual frame intervals.

## Three threads and ownership

* **Window/frontend:** native window, synchronous capture for all mouse buttons,
  clipboard/IME, ImGui frame building and scene edits.
* **Input (`src/engine/input.cpp`):** consumes the NASM SPSC ring, decodes signed
  pointer coordinates and coalesces only consecutive mouse moves. It never calls
  ImGui or mutates a scene/camera being rendered.
* **Render:** scene geometry preparation, Vulkan command recording, fence waits,
  GPU submission and presentation. This is a real persistent worker, not a thread
  spawned and immediately joined per frame.

On Windows the input/render workers are pinned, best effort, to different allowed
**logical CPUs** when the process affinity mask permits at least two. This does
not reserve exclusive physical cores (SMT siblings can share a physical core).
On one-CPU systems the implementation still works without forced affinity.

Only one CPU render job is in flight. `RenderWorker::busy()` uses release/acquire
publication and `wait()` propagates exceptions. While busy, the window procedure
only handles OS lifecycle/capture and enqueues value packets: it does not touch
ImGui, the renderer or scene. After the worker becomes idle, the frontend replays
input, prepares UI and calls `finish_ui_frame()` on the HWND-owning thread. It
then transfers exclusive renderer/scene/ImGui ownership to the worker until the
next completion. The GPU can execute while the frontend pumps native messages
and the input worker preprocesses the next batch. ImGui is deliberately **not**
accessed concurrently; Vulkan command pools/queues have a single CPU owner.

Queued key messages include the keyboard state at receipt, preserving modifiers
through fast Ctrl-down/key/Ctrl-up sequences. Native extra-info tags are also
preserved so delayed replay retains pen/touch mouse-source classification.
Capture is managed immediately by
the native window procedure; the adapted ImGui backend does not recapture the
window when replaying delayed mouse packets. UTF-16/textbox events still use the
Win32 ImGui backend on the frontend thread.

Close/error handling continues pumping native messages while the outstanding GPU
job finishes (avoiding synchronous WSI-message deadlocks), joins workers before
Vulkan shutdown and destroys the HWND only after its Vulkan surface. Render errors are rethrown on the frontend.

## NASM queue ABI and overload

`src/engine/input_queue.asm` implements the actual bounded SPSC producer/consumer
operations for Win64 and ELF64. Each instance owns its storage; no process-global
queue is used. The packet is 320 bytes, capacity 1024; producer/consumer counters
occupy separate cache lines. Win64 nonvolatile registers and SysV AMD64 calling
conventions are preserved. Naturally aligned 64-bit counters and x86 TSO provide
release/acquire publication; there is no locked instruction in the SPSC hot path.
`ENGINE_USE_ASM=OFF` uses equivalent C++ atomics for portability/testing.

The processed buffer is also bounded (4096 events). A full queue does not block
the window procedure and is not silently treated as lossless: it publishes a
sequence cutoff and an explicit reset, reports dropped events, discards older
packets and clears held UI keys/buttons and active drags. Ordinary operation
preserves key/button/wheel/focus ordering, including moves adjacent to clicks.

## Camera

Raw input updates the target orbit. The displayed yaw, pitch and zoom approach it
with exponential damping, `alpha = 1 - exp(-18 * dt)`. Yaw takes the shortest path
across +/-pi. Smoothing uses elapsed seconds, so it behaves consistently at 30,
60 or 240 FPS. Camera snaps/frame-selected synchronize both orbit states.
Ordered press/move/release samples are replayed after current-frame UI gating,
so even a complete drag between two rendered frames is retained. Coalescing
cannot discard the initial press position or start a drag from a UI panel.

## Tests

`input_tests` exercises FIFO/full/wrapping counters, 200,000 concurrent packets,
signed coordinates, event ordering and overflow reset. `render_worker_tests`
checks execution on another thread, backpressure, exception propagation and
joining outstanding work. `camera_smoothing_tests` checks frame-rate independence
and yaw wrapping. `render_worker_gpu_tests` renders real Vulkan UI pixels across
thread handoffs and resizes through both scene and geometry APIs.

Windows CI runs the GPU-free tests; Linux CI runs the full suite with a Vulkan
ICD and covers NASM ON/OFF. Native Windows GUI/driver behavior still needs a
Windows desktop for manual validation.
