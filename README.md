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
- `src/editor/` — edytor Win32 z UI Vulkan oraz terminalowy frontend Linuksa.

Backend tworzy instancję Vulkan, wybiera urządzenie i kolejki, zarządza pamięcią,
buforami wierzchołków, obrazami koloru/głębokości, render passem, pipeline'ami,
command bufferem i synchronizacją. Karty zintegrowane są obsługiwane; dedykowane
mają priorytet. Wymaga kolejki graphics+compute i drawIndirectFirstInstance; nie wymaga geometry shaderów, descriptor indexing ani bindless
textures, ponieważ obecna scena nie ma tekstur i compute cullingu.
Warstwa `VK_LAYER_KHRONOS_validation` i debug messenger włączają się opcjonalnie
w konfiguracji Debug, jeśli są zainstalowane. Ich brak nie blokuje startu.

### Windows

Cały **obszar klienta** głównego okna ma jedną powierzchnię `VkSurfaceKHR` i swapchain.
Nie ma już osobnego HWND dla widoku 3D ani warstwy paneli GDI. Ramkę systemową,
pasek tytułu i natywne okno IME nadal obsługuje Windows/DWM.

```text
Scene → geometria → viewport + scissor 3D → kolor/depth MSAA
UI + font atlas → trójkąty + alpha blending w tym samym render passie
      → resolve GPU → blit GPU do swapchaina → vkQueuePresentKHR
```

**Bez `DrawTextW`, `BitBlt`, `SetDIBitsToDevice` i odczytu pikseli do RAM na Windows.**
`BeginPaint`/`EndPaint` wyłącznie zatwierdzają obszar uszkodzenia Win32.
Zmiana rozmiaru okna odtwarza zasoby; zmiana podziału paneli zmienia viewport,
bez tworzenia okien potomnych. Minimalizacja zatrzymuje rysowanie, a błędy
`OUT_OF_DATE`/`SUBOPTIMAL` odtwarzają swapchain. UI odświeża się co około 16 ms
(timer Win32), aby działały hover, kursor tekstowy i przeciąganie również bez
zmian sceny. To nie jest gwarancja 60 FPS.

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
- Zapis sceny do `scene.txt` (v4: nazwy UTF-8 w cudzysłowach, w tym spacje).
- GPU UI: wyszukiwanie hierarchii, lista ze scrollem, edycja nazw i transformacji,
  rozwijane sekcje inspektora, przyciski, karty zasobów/notatek/diagnostyki.
- Wielowierszowe notatki: Enter/Tab, zaznaczanie, schowek i undo; zapis do
  `project_notes.txt`, ponowne wczytanie przy starcie (limit 8191 bajtów UTF-8).

Testy obejmują scenę, każdy prymityw, zaznaczenie, transformacje, głębokość
niezależną od kolejności obiektów, near clipping, kamerę wewnątrz kostki/kuli,
obiekt za kamerą, powtarzalność obrazu, zmianę rozmiaru i ponowną inicjalizację.
Asercje pozostają aktywne również w konfiguracji Release.

Tekstury materiałów sceny, bindless, compute culling, fizyka, eksport gry i programowanie wizualne
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
z mniejszą kolejką starych klatek; w przeciwnym razie używa FIFO. Edytor składa
teraz scenę i UI w każdej aktywnej klatce, nie używa dawnej bitmapowej pamięci
podręcznej paneli. Nie jest to gwarancja konkretnego FPS — pomiar na
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
(PE/Win32) i Linux (ELF/X11). Główne okno edytora Windows korzysta z tej fabryki;
wejście jest przekazywane do engine-owned backendu UI i sterowania sceną. Frontend Linux edytora
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


## UI / tekst / antyaliasing — kontrakt dla autora edytora

Implementacja UI jest **w engine**, w `src/engine/ui/third_party/imgui/` (Dear ImGui
**v1.91.9b**, MIT, wersja przypięta i dołączona do repo). Do zwykłego buildu nie
potrzeba pobierania ImGui. Edytor buduje layout i zachowanie kontrolek; nie
kompiluje ani nie linkuje drugiej kopii ImGui. `include/engine/ui.hpp` udostępnia
API widgetów, eksportowane z tej samej DLL. Nie podmieniaj nagłówków ani
`imconfig.h` w aplikacji. Licencja jest także w paczce `licenses/Dear-ImGui.txt`.

```cpp
#include "engine/render/vulkan_renderer.hpp"
#include "engine/ui.hpp"

// Po set_window(...) oraz initialize(), jeden raz:
renderer.enable_ui();

// Każda klatka na wątku UI:
renderer.resize(client_width, client_height); // cały obszar klienta, nie sam widok 3D
renderer.begin_ui_frame(delta_seconds);
ImGui::Begin("Panel");
ImGui::TextUnformatted("Tekst renderowany w Vulkanie");
ImGui::Button("Przycisk");
ImGui::End();
renderer.render(scene, camera, selected, {view_x, view_y, view_width, view_height});
renderer.present();
```

- Win32: przed własną obsługą przekazuj **wszystkie** komunikaty do
  `renderer.handle_ui_message(hwnd, message, wp, lp)`. Sterowanie 3D obsługuj
  tylko wewnątrz `RenderViewport`, gdy UI nie przechwytuje wejścia
  (`ImGui::GetIO().WantCaptureMouse/WantCaptureKeyboard/WantTextInput`).
  `WM_MOUSEWHEEL` podaje pozycję ekranową: przelicz ją do klienta. Kółko nad
  panelami przewija panele, nie przybliża kamery. Usuń stare hooki edycji `WM_CHAR`.
- `begin_ui_frame` ustawia kontekst renderera; buduj widgety bezpośrednio po nim.
  `render` zamyka klatkę ImGui i rysuje UI po scenie, przed resolve. Nie wywołuj
  dodatkowego `ImGui::Render()`/backendu Vulkan w edytorze. Jedno render/present
  na klatkę; nie przeplataj kontekstów ani wątków w trakcie budowania layoutu.
- Oryginalne `render(scene, camera, selected)` pozostaje dostępne, także jako
  stary symbol DLL: bez UI, viewport na cały obraz. Gdy rozpoczęto UI frame,
  również ten wariant go kończy. Headless UI działa przez ten sam render pass
  i `pixels()`, lecz nie ma jeszcze linuksowego platform backendu GUI edytora.
- Font Windows: `%WINDIR%/Fonts/segoeui.ttf`, bez kopiowania pliku do repo/paczki.
  Zakres łaciński obejmuje polskie znaki, podstawową interpunkcję i strzałki.
  Jeśli font jest niedostępny, używany jest wbudowany font ImGui (ograniczony
  zestaw znaków). Brak shaping/bidi/CJK/emoji w tym etapie.
- Atlas fontu powstaje raz na CPU z TTF (oversampling 3×2); to **nie jest**
  bitmapa całego UI. Vulkan uploaduje atlas i rysuje glify jako teksturowane
  trójkąty z filtrowaniem liniowym i alpha blending. Tekst, caret, zaznaczenia,
  obramowania i kontrolki nie są malowane przez GDI.
- MSAA jest wspólne dla sceny, gizma i UI. Silnik wybiera **4× → 2× → 1×**,
  przecinając limity urządzenia z obsługą wybranych formatów koloru/głębokości.
  Kolor wielopróbkowy i depth mają tyle samo próbek; resolve trafia do obrazu
  jednoprąbkowego przed blitem/readbackiem. Bez wymogu `sampleRateShading`.
  Rzeczywistą wartość zwraca `antialiasing_samples()`; tekst ma dodatkowo
  pokrycie alpha atlasu, a kształty UI geometryczny antyaliasing ImGui.
- `shutdown()` zwalnia backendy UI/font, kontekst i zasoby Vulkan po ukończeniu
  GPU, przed zniszczeniem HWND. Kolejne `enable_ui()` tworzy czysty kontekst.
- `ui_tests` wymaga sterownika Vulkan, ale nie pulpitu. Sprawdza piksele tekstu,
  pokrycie AA, clipping/scissor, resolve MSAA, kliknięcie przycisku, wpisywanie
  UTF-8, scroll, resize i ponowną inicjalizację. `platform_tests` nadal nie
  wymaga GPU. CI Linux uruchamia wszystkie testy z dostępnym ICD Mesa.

Zgodnie z podziałem pracy: kolejne zmiany layoutu są po stronie `src/editor/`,
backend/atlas/render pass i publiczne API po stronie `src/engine/` + `include/engine/`.
W tej zmianie integracja edytora jest wykonana za jednorazową zgodą na edycję
wszystkich plików. Nie oznacza to przeniesienia logiki edytora do DLL silnika.


## Zwarta geometria i obracanie kamery

Nowe API: [`engine/objects.hpp`](include/engine/objects.hpp), implementacja
`src/engine/objects.cpp`. Wierzchołki HUD (24 B) i 3D (12 B) oraz indeksy HUD/3D
trafiają do jednego VkBuffer w tej kolejności. ObjectData (32 B) i indirect (20 B)
są wyłącznie dla instancji 3D. Compute shader wylicza widoczność; vertex shader
transformuje obiekty. Indirect używa signed vertexOffset zgodnego z Vulkanem.

Kamera korzysta z bieżącej klatki ImGui i kontrolera w engine zamiast starego
filtra WantCaptureMouse w Win32. PPM nad 3D rozpoczyna orbitę; popup i utrata
fokusu są obsługiwane jawnie. Dokładny ABI, pakowanie half/normalnej/rotacji,
przykłady API, wymagania urządzenia i zasady współdzielenia bufora opisuje
[dokumentacja obiektów](docs/objects.md). Testy CPU: geometry_tests; testy
GPU shared-buffer/culling: geometry_gpu_tests.
