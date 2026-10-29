# Obiekty, wspólna geometria GPU i kamera

## Moduły / odpowiedzialność

- `include/engine/objects.hpp`, `src/engine/objects.cpp`: publiczne formaty, konwersje,
  walidacja, budowanie meshów i instancji oraz pakowanie jednego bufora geometrii.
- `src/engine/shaders/`: transformacja instancji w vertex shaderze, HUD, geometria
  pomocnicza oraz rzeczywisty compute culling.
- `include/engine/camera_controller.hpp`, `src/engine/camera_controller.cpp`:
  orbitowanie niezależne od Win32/ImGui, możliwe do testowania bez pulpitu/GPU.
- `src/editor/render.cpp`: podaje wejście kamery z bieżącej klatki ImGui.

## Dokładny ABI

| Typ | Bajty | GPU |
|---|---:|---|
| `hvec4` | 8 | cztery IEEE-754 binary16, w pamięci jako `uint16_t` |
| `Vertex` | 12 | `pos` offset 0, `Normal` offset 8 |
| `Vertex_hud` | 24 | `pos` offset 0 (2 float), `color` offset 8 (4 float) |
| `ObjectData` | 32 | position 0, rotation 12, scale 16, texturePair 20, radius 24, padding 28 |
| `DrawIndexedIndirectCommand` | 20 | zgodny z Vulkanem, w tym **int32_t vertexOffset** |

Używamy własnych `Vec2`, `Vec3`, `Vec4`, a nie zależności GLM. Layout odpowiada
niealignowanym `glm::vec2/vec3/vec4`; nie zależy od `GLM_FORCE_ALIGNED_GENTYPES`.
`static_assert` pilnuje rozmiarów i offsetów. `hvec4` przechowuje bity half, nie
cztery liczby zmiennoprzecinkowe 32-bit. Konwersja jest round-to-nearest-even.
Pozycje muszą być skończone, w zakresie ±65504; `make_vertex` ustawia `w=1`.
Wierzchołki obiektu są lokalne względem jego pivota, nie przeliczone na świat.

Normalna: `VK_FORMAT_A2B10G10R10_UNORM_PACK32`, R/X w bitach 0–9, G/Y 10–19,
B/Z 20–29, A=3 w 30–31. Ponieważ UNORM nie ma znaku, `pack_normal` normalizuje
wektor i mapuje [-1,1] → [0,1]. Shader robi odwrotne mapowanie i normalizację.
Fetch pozycji używa `VK_FORMAT_R16G16B16A16_SFLOAT`; nie potrzeba shaderFloat16.

Rotacja: 10 bitów na każdą oś Euler XYZ, pełen obrót = 1024, bity 30–31 = 0.
`pack_rotation` przyjmuje stopnie (również ujemne i >360), a shader obraca XYZ.
Krok to 0.3515625°, błąd zaokrąglenia do 0.17578125°/oś. Nie jest to quaternion
ani reinterpretacja float. `texture_pair(a,b)` pakuje dwa uchwyty 16-bit; to
metadane dla przyszłych tekstur, **nie ma jeszcze samplowania tekstur materiałów**.
Kolor obiektu jest osobnym parametrem draw, nie ukrytym użyciem texturePair/padding.

`ObjectData.scale` jest skalą jednolitą. Dla istniejących encji edytora ze skalą
niejednolitą silnik skaluje lokalny mesh przed pakowaniem i wysyła scale=1.
Pozycja i spakowana rotacja nadal są stosowane na GPU. Dla skali jednolitej mesh
jest współdzielony pomiędzy instancjami danego prymitywu.
`radius` jest promieniem **w świecie**, wokół `position`. Builder wylicza go z
rzeczywistych half-vertices i abs(scale), konserwatywnie zaokrągla w górę; odrzuca
ręcznie podaną zbyt małą sferę. Padding musi wynosić zero.

## Jeden bufor geometrii — dokładna kolejność

```text
0
[ Vertex_hud × HUD count ]
[ Vertex × 3D count ]
[ uint32 × HUD index count ]
[ uint32 × 3D index count ]
koniec
```

`GeometryLayout` podaje offsety w **bajtach** i liczby elementów. Nie ma nagłówka
ani paddingu pomiędzy sekcjami. Wszystkie sekcje są wyrównane co najmniej do 4 B.
Przekazanie HUD później niż mesha nie zmienia kolejności w wyniku `build()`.
Indeksy meshów są lokalne. `firstIndex` jest offsetem elementów względem sekcji
indeksów 3D, `vertexOffset` — signed offsetem względem sekcji wierzchołków 3D.
`firstInstance` wskazuje indeks w tablicy ObjectData, nigdy offset bajtowy.

Na GPU jest **jeden VkBuffer dla wszystkich pozycji/normali i indeksów**, z usage
VERTEX_BUFFER | INDEX_BUFFER | TRANSFER_DST. Bind vertex/index używa tego samego
uchwytu, ale innych offsetów. Także ImGui przekazuje do niego całą geometrię UI
jako Vertex_hud i indeksy uint32 (konwersja z ImDrawVert/ImDrawIdx); nie tworzy
wtedy swoich osobnych vertex/index bufferów.

Ponieważ żądany Vertex_hud nie ma UV, glify mają dodatkowy strumień UV (8 B na
wierzchołek ImGui). Kolory/oznaczenie siatki i gizma są drugim strumieniem danych
pomocniczych. Te strumienie **nie zawierają osobnych pozycji ani indeksów** i nie
zmieniają czterech sekcji geometrii. Siatka/gizmo używają Vertex i indeksów w
sekcjach 3D, ale nie są instancjami ObjectData ani komendami indirect; ich pozycje
są camera-relative, z osobnym współczynnikiem skali, aby nie przekraczać half.

ObjectData i komendy indirect to osobne, tylko 3D, storage buffers. Compute
shader testuje sfery względem 6 płaszczyzn frustum i zapisuje instanceCount=0/1.
Bariera COMPUTE_SHADER/SHADER_WRITE → DRAW_INDIRECT/INDIRECT_COMMAND_READ poprzedza
`vkCmdDrawIndexedIndirect`. Shader nigdy nie culluje HUD ani tekstu. Instancje
przechodzą transformację na GPU. Render pass/MSAA/tekst nadal działają wspólnie.

Ta ścieżka wymaga Vulkan 1.1, kolejki graphics+compute i funkcji
`drawIndirectFirstInstance`. Nie wymaga multiDrawIndirect: obecnie jest jedno
wywołanie indirect na instancję, by zachować per-object tint. To nie jest jeszcze
jedno multi-draw ani occlusion culling. Brak potrzebnej funkcji daje błąd wyboru
urządzenia, a nie nielegalną komendę Vulkan.
Bufory są host-visible/coherent, zapisywane dopiero po fence poprzedniej klatki;
na Windows nie ma odczytu wyników cullingu ani pikseli do CPU. Headless po fence
udostępnia `last_draw_commands()` dla testów rzeczywistych wyników compute.

## Przykład własnego modelu i HUD

```cpp
#include "engine/objects.hpp"
#include "engine/render/vulkan_renderer.hpp"
#include <array>

engine::GeometryBuilder builder;
std::array<engine::Vertex,3> vertices{
    engine::make_vertex({-.5f,-.5f,0},{0,0,1}),
    engine::make_vertex({ .5f,-.5f,0},{0,0,1}),
    engine::make_vertex({0,.5f,0},{0,0,1})};
std::array<std::uint32_t,3> indices{0,1,2};
auto mesh=builder.add_mesh(vertices,indices);
engine::ObjectData object;
object.position={0,0,0};
object.rotation=engine::pack_rotation({0,45,0});
object.scale=2;
object.texturePair=engine::texture_pair(0,0);
builder.add_object(mesh,object); // promień automatyczny
// builder.add_hud(hud_vertices, hud_indices); // nie dodaje ObjectData/indirect
const auto geometry=builder.build();
renderer.render_geometry(geometry,camera); // również kończy rozpoczętą klatkę UI
renderer.present();
```

Można dodać wiele instancji tego samego mesha; nie powielają wierzchołków/indeksów.
`render(Scene,...)` też używa nowej ścieżki. Typy Scene/Entity pozostają logicznym
API edytora, a ObjectData jest ich zwartą reprezentacją GPU.

## Naprawa kamery

Procedura Win32 przekazuje PPM/ruch/kółko do backendu ImGui, bez drugiej,
natywnej implementacji orbity. Po `begin_ui_frame` i zbudowaniu paneli edytor
podaje bieżące `MousePos`, stan/przyciśnięcie PPM, kółko, fokus i viewport do
`update_orbit`, następnie `apply_orbit`. Nie blokuje orbity globalnym (czasem
należącym do poprzedniej klatki) `WantCaptureMouse` z procedury WM_RBUTTONDOWN.

Start PPM wymaga pozycji w widoku 3D; otwarty popup blokuje start/zoom. Już
rozpoczęte przeciąganie może przejść nad panel, a puszczenie PPM, utrata fokusu
lub nieważna pozycja myszy kończy orbitę. Kółko nad panelami nie zmienia dystansu.
Pitch/dystans są ograniczone; pierwszy klik nie powoduje skoku po poprzednim
położeniu kursora. Capture poza oknem utrzymuje backend Win32 ImGui.

## Testy

- `geometry_tests` — bez GPU: ABI/offsety, wszystkie 63488 skończone kody half,
  normalne/rotacje, indeksy/baseVertex, uszkodzone dane oraz orbita/fokus/popup.
- `geometry_gpu_tests` — Vulkan: HUD i dwa modele z niezerowym firstIndex /
  vertexOffset, 0/1 widoczności faktycznie odczytane po compute, HUD bez instancji,
  ImGui >65535 wierzchołków z VtxOffset i osobnym UV, geometria pusta/HUD-only.
- Istniejące engine_tests i ui_tests pozostają testami regresji sceny i tekstu.
