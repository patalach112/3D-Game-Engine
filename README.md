# Starter 3D Engine

Własny silnik 3D pisany w C++20. Renderer, rasteryzacja trójkątów, bufor głębokości, geometria sceny i uchwyty transformacji są kodem tego projektu. Nie używamy raylib ani innego gotowego silnika. Na Windowsie korzystamy tylko z systemowych API Win32/GDI do okna, tekstu i prezentacji obrazu.

## Struktura projektu

- `include/engine/` — klasy silnika: `Vec3`, `Scene`, `EditorCamera`, `SoftwareRenderer`.
- `src/engine/` — implementacja silnika i własnego renderera.
- `src/editor/` — frontend edytora:
  - `main.cpp` — punkt wejścia `wWinMain`.
  - `editor_app.hpp` — wspólny typ `App` oraz deklaracje funkcji.
  - `app.cpp` — stan edytora, kamera, layout, pola inspektora.
  - `ui.cpp` — widgety GDI: tekst, przyciski, pola, ikony.
  - `render.cpp` — rasteryzacja widoku i backbuffer.
  - `scene_io.cpp` — zapis sceny do `scene.txt`.
  - `interaction.cpp` — strzałka na obiekt, gizmo, dodawanie obiektów.
  - `window.cpp` — procedura okna i klawisze/mouse.
  - `linux_main.cpp` — frontend terminalowy bez X11.
- `tests/engine_tests.cpp` — testy sceny i renderera.

## Windows

Wymagane: CMake 3.20+ oraz kompilator C++20, np. Visual Studio 2022 albo MinGW-w64.

```powershell
cmake -S . -B build
cmake --build build --config Release
# Visual Studio:
.\build\Release\engine_editor.exe
# MinGW/Ninja:
.\build\engine_editor.exe
```

W konfiguracji MinGW CMake dodaje `-municode`, aby linker poprawnie użył wejściowej funkcji `wWinMain`.

## Linux

Na Linuxie ten sam rdzeń sceny i własny renderer uruchamiają testowy edytor terminalowy. Pokazuje podgląd 3D w terminalu obsługującym kolory true-color i zapisuje pełny obraz do `scene_preview.ppm`. Wersja ta nie wymaga X11 ani zewnętrznych bibliotek graficznych, dzięki czemu może służyć do budowania i testowania renderera również w środowisku bez pulpitu. Dla generatorów jednokonfiguracyjnych, takich jak Ninja/MinGW, zwykła konfiguracja domyślnie włącza Release z optymalizacją; do debugowania ustaw `-DCMAKE_BUILD_TYPE=Debug`.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/engine_editor
```

Komendy wersji terminalowej: `add`, `select ID`, `move X Y Z`, `delete`, `save`, `render`, `help`, `quit`.

## Prototyp zawiera

- Sceny z kostkami, kulami, walcami i płaszczyznami 3D.
- Inspektor z edytowalnymi polami pozycji, obrotu i skali. Kliknij wartość, wpisz liczbę i naciśnij Enter.
- Własny renderer programowy z perspektywą, rasteryzacją ścian, testem głębokości i przeciąganiem ścian przez płaszczyznę bliskości — duże obiekty, w których kamera się znajduje, tracą już nie ściany.
- Siatkę bez wypełnionej podłogi: tło widoku prześwituje pomiędzy liniami. Linie mają poprawioną interpolację głębokości, delikatne wygaszanie i większą odporność na znikanie przy przecięciach.
- Windowsowy interfejs rysowany do bufora pozaekranowego i odświeżany tylko po zmianie sceny lub okna, by ograniczyć miganie i niepotrzebne obciążenie CPU.
- Windowsowy, skalowalny interfejs z hierarchią i inspektorem, wyświetlający tekst Unicode w Segoe UI. Główne okno można zmieniać rozmiarem, a szerokość paneli zmienić przez przeciągnięcie separatorów.
- Interfejs rysowany offline i kopowany na krawędzi paneli: zapasowy bufor GDI, cached będące przyciski/pola i pojedynczy tekst paska statusu to wszystko, co odświeżane jest w każdym kadrze. Przyciski, pola i ikony tworzą swoje obiekty GDI tylko raz, a nie przy każdym odwróceniu.
- Zaznaczanie i przesuwanie obiektów kolorowymi uchwytami osi.
- Obracanie kamery prawym przyciskiem myszy i przybliżanie kółkiem.
- Zapis sceny do `scene.txt`.

Wersja terminalowa Linuksa służy do sprawdzania rdzenia i renderera. Nie zastępuje jeszcze graficznego edytora Windows. Fizyka, edytor kodu C++, programowanie wizualne oraz eksport gotowej gry są dalszymi etapami.
