# Starter 3D Engine — Vulkan + shared engine

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
zaznaczenia lub rozmiaru. Odsłonięcie widoku prezentuje zapisany obraz, a zmiana
samego UI nie wymusza prezentacji GPU.
Backend współdzieli jeden zestaw zasobów klatki, chroniony fence przed ponownym
użyciem. Semafory prezentacji są osobne na obraz swapchaina. Nie blokuje CPU po
każdej prezentacji, ale nie jest jeszcze wieloklatkowym rendererem gry.

### Linux / headless

Ten sam backend Vulkan renderuje do obrazu pozaekranowego, kopiuje go do bufora
host-visible i udostępnia `pixels()` jako `0x00RRGGBB`. Terminal wyświetla podgląd
72×48 w ANSI true-color i zapisuje ten sam obraz do `scene_preview.ppm`.
Terminal nie otwiera okna, ale **wymaga działającego sterownika Vulkan**.
Natywne okna udostępnia osobny backend X11/XWayland; build czysto headless można
skonfigurować z `-DENGINE_LINUX_X11=OFF` (wtedy nie wymaga nagłówków X11).
Można użyć sprzętowego ICD albo programowego ICD (np. Mesa lavapipe / SwiftShader)
w CI. Programowy ICD nadal wykonuje polecenia Vulkan; projekt nie ma fallbacku
na poprzedni rasteryzator CPU. `pixels()` nie jest zapełniane w trybie swapchaina.

## Budowanie

Wymagane: CMake 3.20+, kompilator C++20, NASM na x86-64, nagłówki/biblioteka Vulkan 1.1+,
**glslc albo glslangValidator** do kompilacji shaderów oraz sterownik Vulkan.

### Windows

Zainstaluj Vulkan SDK, NASM (dostępny w PATH) i sterownik GPU z obsługą Vulkan. Uruchom nowy terminal,
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
sudo apt install cmake g++ nasm libx11-dev libvulkan-dev glslang-tools mesa-vulkan-drivers
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

## Przekazanie EXE innemu użytkownikowi (MinGW)

Domyślnie `ENGINE_STATIC_MINGW_RUNTIME=ON`: GCC, libstdc++ oraz potrzebny runtime
wątków są linkowane statycznie. Po **ponownym zbudowaniu** nie trzeba przekazywać
`libgcc_s_seh-1.dll`, `libstdc++-6.dll` ani `libwinpthread-1.dll`.
Nie pobieraj tych DLL-i z przypadkowych stron internetowych.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENGINE_STATIC_MINGW_RUNTIME=ON
cmake --build build --config Release
ctest --test-dir build -C Release -R engine_runtime_dependencies --output-on-failure
cmake --build build --config Release --target package
```

Koledze przekaż nowo utworzone archiwum `build\Starter3DEngine-*.zip`, a nie EXE
ze starego buildu. W ZIP-ie jest program, **engine.dll** i instrukcja; shadery są wbudowane.
Po przeniesieniu silnika do biblioteki samo EXE już nie wystarczy — DLL musi
leżeć obok EXE, a na Linuxie analogicznie `libengine.so*`.
Test importów PE oraz pakowanie ZIP-a wykonuje też workflow Windows MinGW.
Opcja dotyczy MinGW, nie toolchaina MSVC. Można ją wyłączyć przez `OFF`, ale wtedy
trzeba samodzielnie dostarczyć zgodne biblioteki runtime kompilatora.

Na komputerze odbiorcy **nadal wymagany jest sterownik GPU z obsługą Vulkan 1.1+**
(w tym `vulkan-1.dll`). Nie jest potrzebny Vulkan SDK. Nie pakujemy sterownika
z komputera dewelopera do archiwum.

### Płynność viewportu

Na Windowsie zapis sceny i blit do swapchaina są teraz częścią **jednego zgłoszenia
GPU**. Nie ma czekania na fence pomiędzy rasteryzacją a blitem ani
`vkQueueWaitIdle` po każdej prezentacji. Fence przed kolejnym użyciem zasobów
pozostaje — chroni bufor wierzchołków, command buffer i współdzielony depth buffer.
Headless nadal czeka na zakończenie odczytu pikseli. Pełne oczekiwanie na urządzenie
jest zachowane przy zmianie zasobów/rozmiaru i zamykaniu aplikacji.

Jeśli sterownik obsługuje MAILBOX, renderer wybiera go dla prezentacji bez tearingu
z mniejszą kolejką starych klatek; w przeciwnym razie używa FIFO. Edycja samego UI
nie prezentuje ponownie niezmienionego viewportu. Odsłonięcie/przywrócenie okna
wciąż wymusza prezentację. Nie jest to gwarancja konkretnego FPS — pomiar na
rzeczywistym GPU jest nadal potrzebny, szczególnie dla dużych scen.


## Platforma i biblioteka silnika

Cały kompilowany kod `src/engine/` jest teraz w celu CMake **`engine_core SHARED`**:
Windows tworzy `engine.dll` i bibliotekę importową, Linux `libengine.so` (SONAME 1).
Edytor oraz testy linkują tę bibliotekę; nie kompilują jej implementacji ponownie.
Pakowanie/install dołącza bibliotekę. Zainstalowany edytor Linux szuka jej także
obok programu przez RPATH `$ORIGIN`.

Publiczne nagłówki:
- `include/engine/platform.hpp` — wybór backendu przez `_WIN32` / `__linux__`;
- `windows.hpp`, `linux.hpp` — deklaracje odpowiednich fabryk;
- `platform_types.hpp` — wspólne okno, konfiguracja, uchwyty i callbacki;
- `export.hpp` — eksport/import DLL (`ENGINE_API`).

Implementacje: `src/engine/windows.cpp`, `linux.cpp`, `platform.cpp` i `call.asm`.
CMake kompiluje **tylko właściwy** plik natywny. Użycie z dowolnej platformy:

```cpp
#include "engine/platform.hpp"

engine::WindowConfig config;
config.title = "Moje okno"; // UTF-8
config.width = 1280;
config.height = 720;
auto window = engine::createnewwindow(config);
while (!window->should_close()) {
    window->poll_events();
    // logika / renderowanie; w prawdziwej pętli także ograniczanie tempa
}
```

Nie jest to rozpoznawanie systemu przez ten sam EXE: osobno buduje się Windows
(PE/Win32) i Linux (ELF/X11). Okna edytora Windows korzystają z tej fabryki,
zachowując dotychczasowe hooki wejścia i viewport Vulkan. Frontend Linux edytora
pozostaje terminalowy. Przykład samego natywnego okna na obu systemach:

```sh
cmake -S . -B build -DENGINE_BUILD_WINDOW_EXAMPLE=ON
cmake --build build --config Release
# Linux: ./build/platform_window_example
# Windows VS: .\build\Release\platform_window_example.exe
# Windows MinGW: .\build\platform_window_example.exe
```

X11 potrzebuje `DISPLAY`, np. przez XWayland. W buildzie headless wywołanie
`createnewwindow` zgłasza czytelny błąd zamiast udawać, że utworzyło okno.
Backend Linux nie obsługuje hooków komunikatów Win32 ani okien potomnych; odrzuca
te opcje. Zwrócone uchwyty są pożyczone i ważne tylko podczas życia okna.
Operacje okienne i callbacki wykonuj na wątku UI. Renderer zwolnij **przed** oknem.
Nie przechowuj wyjątków ani surowych obiektów z DLL po jej wyładowaniu.

### Dostosowanie ASM

`src/engine/call.asm` jest NASM x86-64, z formatem `win64` lub `elf64` wybieranym
przez CMake. Na Windows `WindowProc` przekazuje wywołanie do kompilowanego
`engine_window_proc`. To tail-call bez modyfikacji stosu/rejestrów: zachowuje ABI,
shadow space i wyrównanie. Natywna implementacja zajmuje się signed współrzędnymi
myszy, minimalizacją, zamykaniem i opcjonalnym borderless hit-testem.

Zamiast globalnej tabeli 1025 wskaźników, globalnego RECT i adresu bajtu używanego
jako callback, każde okno ma własny stan. Callback zmiany rozmiaru jest funkcją,
a nie wskaźnikiem na flagę. Nie zachowujemy niebezpiecznych sygnatur starego ASM:

```cpp
void* memory = createheap(8200); // liczba BAJTÓW, pamięć wyzerowana; nullptr przy błędzie/0
freeheap(memory);               // zawsze tym API, nie delete/free po stronie EXE
WindowResizedCallback(window.get(),
    [](engine::WindowSize size, bool minimized, void* user) { /* ... */ }, nullptr);
```

Linux używa ABI System V i wywołań PIC przez PLT, Windows ABI Win64. Funkcje C++
wybierają `HeapAlloc/HeapFree` lub `calloc/free` i nie przepuszczają wyjątków przez
most ASM. Nie ma windowsowych importów w obiekcie ELF. `WindowProc` nie istnieje
na Linuxie, bo X11 nie używa procedury WNDPROC.

`ENGINE_USE_ASM=ON` jest domyślne na x86-64 i wymaga NASM. Bez NASM albo na innej
architekturze można użyć `-DENGINE_USE_ASM=OFF`: ten sam interfejs implementuje
fallback C++ (nie oznacza to emulacji Win32 na Linuxie).

API klas (`Scene`, `VulkanRenderer`, `Window`) jest API **C++**, nie stabilnym ABI
pluginów: EXE i bibliotekę buduj tym samym toolchainem, architekturą i konfiguracją
runtime. Nie mieszaj MSVC/MinGW ani różnych wersji standardowej biblioteki C++.

Test `platform_tests` nie wymaga GPU: sprawdza eksportowane funkcje pamięci,
obsługę null i odrzucanie błędnych parametrów. `platform_tests --window` dodatkowo
otwiera dwa niezależne okna i sprawdza zamykanie; potrzebuje pulpitu lub Xvfb.
CI Linux testuje X11 z Xvfb i warianty NASM/C++; CI MinGW sprawdza także importy
zarówno EXE, jak i engine.dll, by nie przeoczyć zależności runtime w bibliotece.
