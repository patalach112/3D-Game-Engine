# Starter 3D Engine — Vulkan

Własny silnik/edytor 3D w C++20 z **rendererem Vulkan 1.1**. Geometria sceny,
transformacje, kamera, zaznaczanie i gizmo należą do projektu. Rasteryzację,
clipping, interpolację, cieniowanie i test głębokości wykonuje Vulkan — nie pętle
rysujące piksele na CPU. Nie używamy raylib, OpenGL ani Direct3D.

## Architektura renderowania

- `src/engine/software_renderer.cpp` — **pełna implementacja backendu Vulkan**;
  historyczna nazwa pliku została zachowana, ale stary rasteryzator CPU usunięto.
- `include/engine/render/vulkan_renderer.hpp` — `VulkanRenderer`, kamera i funkcje
  projekcji/ray picking. Nagłówek nie ujawnia typów Vulkan ani Windows.
- `include/engine/render/software_renderer.hpp` — wyłącznie alias kompatybilności
  `SoftwareRenderer = VulkanRenderer`, nie alternatywny renderer.
- `src/shaders/scene.vert`, `scene.frag` — shadery GLSL. CMake kompiluje je do
  SPIR-V i osadza w programie; uruchomienie nie wymaga plików shaderów ani
  określonego katalogu roboczego. Nie potrzeba GLM ani shaderc w runtime.
- `src/engine/scene.cpp`, `include/engine/scene/` — płaska lista encji i transformacje.
- `src/editor/` — edytor Win32/GDI oraz terminalowy frontend Linuksa.

Backend tworzy instancję Vulkan, wybiera urządzenie i kolejki, zarządza pamięcią,
buforami wierzchołków, obrazami koloru/głębokości, render passem, pipeline'ami,
command bufferem i synchronizacją. Karty zintegrowane są obsługiwane; dedykowane
mają priorytet. Nie wymaga geometry shaderów, descriptor indexing ani bindless
textures, ponieważ obecna scena nie ma tekstur i compute cullingu.
Warstwa `VK_LAYER_KHRONOS_validation` i debug messenger włączają się opcjonalnie
w konfiguracji Debug, jeśli są zainstalowane. Ich brak nie blokuje startu.

### Windows

Widok sceny ma osobne okno potomne Win32 z `VkSurfaceKHR` i swapchainem. Przepływ:

```text
Scene → geometria → vertex buffer → shadery + depth buffer
      → obraz GPU → blit GPU do swapchaina → vkQueuePresentKHR
```

**Nie ma `SetDIBitsToDevice` ani odczytu pikseli do RAM w ścieżce Windows.**
GDI nadal rysuje panele, tekst i przyciski edytora, nie scenę 3D.
Zmiana rozmiaru okna/separatorów odtwarza potrzebne zasoby. Minimalizacja pomija
renderowanie/prezentację, a `VK_ERROR_OUT_OF_DATE_KHR` i `VK_SUBOPTIMAL_KHR`
powodują odtworzenie swapchaina. Zasoby są zwalniane po zakończeniu pracy GPU.

Edytor jest odświeżany zdarzeniowo: rasteryzacja tylko po zmianie sceny, kamery,
zaznaczenia lub rozmiaru; przy samym odmalowaniu UI prezentuje gotowy obraz.
Na razie jest **jedna klatka w locie**, z fence i semaforami prezentacji osobnymi
na obraz swapchaina. To prosty, zsynchronizowany backend edytora, nie jeszcze
wieloklatkowy renderer do gry o wysokiej przepustowości.

### Linux / headless

Ten sam backend Vulkan renderuje do obrazu pozaekranowego, kopiuje go do bufora
host-visible i udostępnia `pixels()` jako `0x00RRGGBB`. Terminal wyświetla podgląd
72×48 w ANSI true-color i zapisuje ten sam obraz do `scene_preview.ppm`.
Nie wymaga X11/Wayland ani okna, ale **wymaga działającego sterownika Vulkan**.
Można użyć sprzętowego ICD albo programowego ICD (np. Mesa lavapipe / SwiftShader)
w CI. Programowy ICD nadal wykonuje polecenia Vulkan; projekt nie ma fallbacku
na poprzedni rasteryzator CPU. `pixels()` nie jest zapełniane w trybie swapchaina.

## Budowanie

Wymagane: CMake 3.20+, kompilator C++20, nagłówki/biblioteka Vulkan 1.1+,
**glslc albo glslangValidator** do kompilacji shaderów oraz sterownik Vulkan.

### Windows

Zainstaluj Vulkan SDK i sterownik GPU z obsługą Vulkan. Uruchom nowy terminal,
żeby CMake widział `VULKAN_SDK` i narzędzia SDK. Przykład z Visual Studio 2022:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\engine_editor.exe
```

Dla MinGW/Ninja ścieżka to `build\engine_editor.exe`; CMake dodaje `-municode`
dla `wWinMain`. Użyj biblioteki loadera Vulkan pasującej do toolchaina/architektury.
Na maszynie docelowej wymagany jest `vulkan-1.dll` dostarczany ze sterownikiem.

### Linux

Przykładowe pakiety Debiana/Ubuntu:

```sh
sudo apt install cmake g++ libvulkan-dev glslang-tools mesa-vulkan-drivers
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/engine_editor
```

Dla niestandardowego SDK ustaw `VULKAN_SDK` / `CMAKE_PREFIX_PATH`; kompilator
shaderów można wskazać przez `-DENGINE_GLSLC=/ścieżka/glslc` lub
`-DENGINE_GLSLANG=/ścieżka/glslangValidator`. Testy nie pomijają braku sterownika:
bez poprawnego ICD uruchomienie zakończy się błędem inicjalizacji Vulkan.

Komendy terminala: `add`, `select ID`, `move X Y Z`, `delete`, `save`, `render`,
`help`, `quit`.

## Funkcje

- Kostki, kule, walce i płaszczyzny; mesh każdego typu jest generowany i buforowany.
- Pozycja, obrót Euler XYZ i skala, również ujemna.
- Perspektywa z płaszczyzną near 0.08 i far 100000; clipping wykonuje Vulkan.
- Dwustronne ściany, także gdy kamera znajduje się wewnątrz bryły.
- Proste płaskie cieniowanie kierunkowe, kolory brył, krawędzie zaznaczenia.
- Siatka XZ bez nieprzezroczystej podłogi, adaptacyjny rozstaw i wygaszanie w oddali.
- Gizmo osi X/Y/Z jako nakładka Vulkan, ze stałym ekranowym rozmiarem końcówek.
- Windows: hierarchia, inspektor, przeciągane separatory paneli, Unicode/Segoe UI,
  obracanie kamery PPM, zoom kółkiem, zaznaczanie i przeciąganie obiektów.
- Zapis sceny do `scene.txt`.

Testy obejmują scenę, każdy prymityw, zaznaczenie, transformacje, głębokość
niezależną od kolejności obiektów, near clipping, kamerę wewnątrz kostki/kuli,
obiekt za kamerą, powtarzalność obrazu, zmianę rozmiaru i ponowną inicjalizację.
Asercje pozostają aktywne również w konfiguracji Release.

Tekstury, bindless, compute culling, fizyka, eksport gry i programowanie wizualne
nie są jeszcze zaimplementowane.
