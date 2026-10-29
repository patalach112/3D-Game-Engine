# Dear ImGui (engine-owned)

- Upstream: https://github.com/ocornut/imgui
- Pinned release: **v1.91.9b**, MIT license (`imgui/LICENSE.txt`).
- Vendored: core, stb font/geometry helpers, Vulkan and Win32 backends only.
- Local change: the end of `imconfig.h` defines `IMGUI_API ENGINE_API` and disables
  obsolete functions. This exports one implementation from the shared engine.
- No network access or additional package is needed during a normal project build.
- Do not build a second copy in the editor, or use headers from a different release.
- Microsoft font files are NOT vendored: Segoe UI is read from Windows/Fonts at runtime.
