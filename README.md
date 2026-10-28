# Starter 3D Engine

Własny edytor i silnik 3D na komputery PC. Silnik oraz kod tworzonych gier piszemy w C++. Celem jest narzędzie, które z czasem połączy wygodny edytor wizualny z normalnym kodowaniem.

## Co już działa

- Okno edytora 3D z panelem sceny i inspektorem obiektu.
- Widok perspektywiczny z siatką i kostkami 3D.
- Dodawanie kostek przyciskiem albo klawiszem `N`.
- Zaznaczanie obiektów w widoku 3D oraz na liście sceny.
- Przesuwanie zaznaczonego obiektu strzałkami i klawiszami `Page Up` / `Page Down`.
- Obracanie kamery prawym przyciskiem myszy i przybliżanie kółkiem.
- Usuwanie zaznaczonego obiektu klawiszem `Delete`.
- Zapisywanie podstawowej sceny do `scene.txt`.
- Testy podstawowego modelu sceny i transformacji.

To wczesny prototyp. Nie ma jeszcze edytora kodu, programowania wizualnego, fizyki ani eksportu samodzielnej gry. Te elementy będą dokładane etapami.

## Wymagania

- CMake 3.20 lub nowszy
- Kompilator C++20, np. Visual Studio 2022 na Windows
- Połączenie z internetem przy pierwszej konfiguracji, aby CMake pobrał bibliotekę raylib z GitHub

## Budowanie na Windows

Otwórz PowerShell w folderze repozytorium i wykonaj:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\engine_editor.exe
```

CMake pobiera raylib automatycznie podczas pierwszej konfiguracji. Po uruchomieniu edytora scena może być zapisana przyciskiem `Zapisz scene`; plik `scene.txt` pojawi się w bieżącym katalogu uruchomienia programu.

## Skróty edytora

- `N` — dodaj kostkę
- `Delete` — usuń zaznaczony obiekt
- Strzałki — przesuń obiekt w poziomie
- `Page Up` / `Page Down` — przesuń obiekt w pionie
- Prawy przycisk myszy i ruch — obracaj kamerę
- Kółko myszy — przybliż / oddal widok

## Testy

```powershell
ctest --test-dir build -C Release --output-on-failure
```

## Struktura

- `include/engine/` — publiczne typy silnika, matematyka i scena
- `src/engine/` — implementacja rdzenia
- `src/editor/` — aplikacja edytora 3D
- `tests/` — testy automatyczne
