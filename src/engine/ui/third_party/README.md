# Dear ImGui (engine-owned)

- Upstream: https://github.com/ocornut/imgui
- Pinned release: **v1.91.9b**, MIT license (`imgui/LICENSE.txt`).
- Vendored: core, stb font/geometry helpers, Vulkan and Win32 backends only.
- Local change: the end of `imconfig.h` defines `IMGUI_API ENGINE_API` and disables
  obsolete functions. This exports one implementation from the shared engine.
- No network access or additional package is needed during a normal project build.
- Do not build a second copy in the editor, or use headers from a different release.
- Microsoft font files are NOT vendored: Segoe UI is read from Windows/Fonts at runtime.

Engine Vulkan backend changes: optional `SharedGeometry` initialization and
`ImGui_ImplVulkan_SetSharedGeometry` bind the common geometry buffer (Vertex_hud,
uint32 indices) plus a separate UV stream. The default upstream ImDrawVert path
is preserved when the option is false. Shared buffers remain engine-owned and
are never destroyed by the ImGui backend. See docs/objects.md.
