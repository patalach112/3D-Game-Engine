# Starter 3D Engine

Własny edytor i silnik 3D na komputery PC. Silnik oraz kod tworzonych gier piszemy w C++. Projekt rozwijamy etapami, zaczynając od edytora sceny 3D.

## Co działa w prototypie

- Ciemny interfejs z hierarchią sceny i inspektorem obiektu.
- Czcionka Inter z obsługą polskich znaków. Jej licencja OFL 1.1 znajduje się w `assets/licenses/Inter-OFL.txt`.
- Widok sceny 3D z delikatniejszą siatką i kamerą perspektywiczną.
- Dodawanie kostek przyciskiem lub klawiszem `N`.
- Zaznaczanie obiektów w widoku sceny albo na liście.
- Uchwyt przesuwania z kulkami na obu końcach osi X, Y i Z. Złap kulkę i przeciągnij ją, aby przesunąć obiekt wzdłuż osi.
- Obracanie kamery prawym przyciskiem myszy. Ruch myszy w górę przesuwa teraz widok w górę.
- Przybliżanie i oddalanie kółkiem myszy.
- Usuwanie zaznaczonego obiektu klawiszem `Delete`.
- Zapisywanie podstawowej sceny do pliku `scene.txt`.

To wciąż wczesny prototyp. Edytor kodu C++, programowanie wizualne, fizyka oraz eksport gotowej gry nie są jeszcze zaimplementowane.

## Wymagania

- CMake 3.20 lub nowszy
- Kompilator C++20, np. Visual Studio 2022 na Windows
- Połączenie z internetem przy pierwszej konfiguracji, aby CMake pobrał raylib z GitHub

## Budowanie na Windows

Otwórz PowerShell w folderze repozytorium i wykonaj:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\engine_editor.exe
```

CMake pobierze raylib automatycznie i skopiuje pliki czcionki obok programu. Po uruchomieniu edytora przycisk `Zapisz scenę` utworzy plik `scene.txt` w bieżącym katalogu.

## Sterowanie

- `N` — dodaj kostkę
- `Delete` — usuń zaznaczony obiekt
- Przeciągnij kolorową kulkę uchwytu — przesuń obiekt wzdłuż osi
- Prawy przycisk myszy + ruch — obracaj kamerę
- Kółko myszy — przybliż / oddal widok

## Testy

```powershell
ctest --test-dir build -C Release --output-on-failure
```
