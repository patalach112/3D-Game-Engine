# Starter 3D Engine

Projektujemy własny silnik 3D w C++20. Nie używamy raylib, Unity ani Unreal jako silnika. Renderer 3D jest napisany w tym repozytorium: rzutuje geometrię, rasteryzuje trójkąty, obsługuje bufor głębokości oraz rysuje siatkę i uchwyty transformacji. Na Windowsie do utworzenia okna i wyświetlenia obrazu używamy natywnego Win32 i GDI.

## Co działa w prototypie

- Własny programowy renderer 3D w `src/engine/software_renderer.cpp`.
- Scena z kostkami, perspektywą, oświetleniem ścian i buforem głębokości.
- Podłoga nie jest wypełnioną, nieprzezroczystą płaszczyzną. Widać tylko delikatną siatkę, a tło sceny prześwituje pomiędzy liniami.
- Edytor Windows z hierarchią sceny i inspektorem.
- Zaznaczanie kostek i przesuwanie ich kolorowymi uchwytami na osiach X, Y i Z.
- Obracanie kamery prawym przyciskiem myszy, przybliżanie kółkiem.
- Interfejs rysowany natywną czcionką Segoe UI. Napisy są wyświetlane w Unicode, więc polskie znaki działają poprawnie. Rozmiar tekstu dopasowuje się do wysokości okna.
- Zapisywanie sceny do pliku `scene.txt`.

To nadal wczesny prototyp. Nie ma jeszcze fizyki, edytora kodu użytkownika, programowania wizualnego ani eksportu samodzielnych gier.

## Budowanie na Windows

Wymagane: CMake 3.20+ oraz Visual Studio 2022 z obsługą C++20.

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\engine_editor.exe
```

Edytor nie pobiera żadnych zewnętrznych bibliotek graficznych. Używa wyłącznie kodu silnika z repozytorium i systemowych API Windows.

## Sterowanie

- `N` — dodaj kostkę
- `Delete` — usuń zaznaczony obiekt
- Przeciągnij kolorową kulkę osi — przesuń zaznaczoną kostkę wzdłuż osi
- Prawy przycisk myszy + ruch — obracaj kamerę
- Kółko myszy — przybliż lub oddal widok
- Przycisk „Zapisz scenę” — zapisz scenę do `scene.txt`

## Testy rdzenia i renderera

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Renderer i model sceny mogą być kompilowane oraz testowane także na innych platformach. Graficzny edytor w tym prototypie korzysta z natywnego API Windows.
