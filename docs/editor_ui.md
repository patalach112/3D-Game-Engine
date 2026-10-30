# Edytor: panele, pasek tytułu i konsola

## Układ

- Hierarchia po lewej, inspektor po prawej, scena w środku, konsola na dole.
- Przeciągnij zakładkę lub nagłówek, aby odpiąć panel albo zadokować go przy
  krawędzi/innej zakładce. Separatory zmieniają rozmiary paneli.
- Zasoby, Notatki i Renderer są dodatkowymi zakładkami dolnego obszaru.
- Menu **Widok** przywraca zamknięte panele; **Przywróć układ** resetuje docking.
- Projektant zapisuje układ w `editor_hud.layout` obok EXE (patrz niżej). Nie używamy
  wielookienkowych platform viewports: wszystkie panele pozostają w jednym HWND
  i na jednym docelowym obrazie Vulkan.

## Pasek i menu

Pasek jest rysowany przez Vulkan. Pusty fragment obok nazwy aplikacji służy do
przesuwania okna; dwuklik maksymalizuje/przywraca okno. Przyciski po prawej:
minimalizacja, maksymalizacja/przywracanie, zamknięcie. Krawędzie i narożniki
obsługują zmianę rozmiaru. Maksymalizacja respektuje obszar roboczy monitora.

- **Plik**: Import/Eksport są na razie szare i nieaktywne; zapis sceny,
  Ustawienia oraz zamknięcie działają.
- **Narzędzia**: Przesuwanie (Q), Obracanie (W), Skalowanie (E).
  Widok sceny zawiera te same przełączniki. Ruch ma strzałki, obrót pierścienie,
  a skala sześcienne uchwyty. Przeciąganie zmienia wybraną transformację.
- **Dodaj → Obiekty**: Kostka, Kula, Walec, Płaszczyzna 3D (podłoże XZ).
- **Dodaj → Płaszczyzna**: Koło 2D i Kwadrat 2D (płaskie siatki XY, bez grubości).
  Podmenu otwierają się po najechaniu, nie wymagają dodatkowego kliknięcia.

Koło i kwadrat obsługują transformacje, zaznaczanie, duplikowanie oraz zapis do
`scene.txt`. Nowe typy dopisano za dotychczasowymi numerami prymitywów.
Picking płaskich figur uwzględnia obrót i skalę; koło nie jest kulą ani
prostopadłościanem z ukrytą grubością.

## Logi i komendy

Historia silnika jest chroniona mutexem i ograniczona do 2048 wpisów (4096
znaków wiadomości / 96 znaków tagu). INFO/WARN/ERROR działają także w Release;
DEBUG jest wtedy kompilowany jako no-op. Windows zapisuje `engine.log`, Linux
korzysta ze stderr. Ostrzeżenia walidacji Vulkan trafiają również do historii.

Konsola udostępnia filtr tekstowy (także `-wykluczenie`), checkboxy poziomów,
kopiowanie widocznych wpisów, lokalne czyszczenie i automatyczne przewijanie.
Przewinięcie w górę nie jest przerywane przez nowe wpisy. Kopiowanie/czyszczenie
nie modyfikuje pliku logu. Wpisy są pobierane przyrostowo i formatowane raz,
a lista korzysta z clippera zamiast renderować każdy wiersz co klatkę.

Komendy zatwierdza się Enterem:

```
help
clear
stats
grid on
grid off
camera reset
```

Nie jest to powłoka systemowa: nie uruchamia procesów ani dowolnych poleceń OS.

## Integracja Vulkan / wątki

Viewport 3D pochodzi z rzeczywistego obszaru treści panelu, z wyłączeniem paska
narzędzi. Zamknięta, zwinięta lub nieaktywna zakładka sceny nie renderuje sceny
na cały ekran. Panele/popupy przed sceną blokują rozpoczęcie interakcji kamerą.

Scena pozostaje bezpośrednim rysowaniem Vulkan, a nie bitmapą CPU. Dla odpiętego
panelu sceny dzielimy scissors komend UI znajdujących się **za** nim, aby ich
tła nie przykryły obrazu 3D. Elementy przed sceną zachowują zwykły porządek Z.
Przygotowanie UI, finalizacja i ewentualna korekta scissors odbywają się przed
oddaniem wyłącznej własności wątkowi renderującemu. Callback HWND nie dotyka
ImGui: hit-test paska używa wyłącznie wcześniej wyliczonego prostokąta.

## Weryfikacja

- `log_tests`: Release, filtrowanie, limit historii, sekwencje, równoległe zapisy
  i migawki, długości wiadomości.
- `docking_tests`: domyślny układ, widoczna konsola, przezroczystość sceny,
  zmiana dokowania, zapis/odczyt układu i reset (bez GPU).
- `primitives_gpu_tests`: prawdziwy Vulkan; koło kontra kwadrat, figura widziana
  krawędzią, trzy wyglądy gizma, scena zadokowana i odpięta bez zasłaniania tłem UI.
- Windows `editor_state_tests`: płaski picking, trafianie w uchwyty/pierścienie,
  zbudowanie rzeczywistego UI bez GPU, zamykanie sceny, wejście komendy konsoli
  oraz otwarcie ustawień.

Przy ręcznym teście Windows sprawdź dodatkowo przeciąganie/dwuklik paska,
resize wszystkich krawędzi, maksymalizację przy pasku zadań i na dodatkowym
monitorze oraz zamknięcie podczas aktywnej pracy GPU.

## Projektant `editor_hud.exe`

Oba EXE korzystają z **tego samego frontendu i renderera Vulkan**. Projektant
zaczyna od pełnego domyślnego interfejsu: menu, scena, hierarchia, inspektor,
konsola, zasoby, notatki, renderer i ustawienia. Przełączniki ruch/obrót/skala
nad sceną są zwykłymi edytowalnymi przyciskami dokumentu. Dynamiczna zawartość
narzędzi (np. encje i logi) pozostaje wbudowaną funkcjonalnością paneli, a nie
kopią danych sceny w pliku HUD.

1. Uruchom `editor_hud.exe`, zbudowany obok `engine_editor.exe` i `engine.dll`.
2. LPM zaznacza element podglądu; drzewo **HUD / Właściwości** pozwala wybrać
   także ukryte okna, menu i zagnieżdżone listy.
3. PPM otwiera menu kontekstowe. Pierwszą pozycją jest **Dodaj okno**. Podmenu
   **Dodaj** zawiera elementy dozwolone w danym kontenerze. **Przycisk** jest
   dostępny tylko po trafieniu w panel, nie na pustym obszarze ani nagłówku
   okna. Do pustego okna najpierw dodaj panel. Elementy list/menu dodaje się
   również przez **Dodaj pozycję listy** we właściwościach listy.
4. W panelu **Layout** ustaw odstęp przycisków X/Y, odstęp od krawędzi X/Y oraz
   minimalną wielkość przycisków. Przyciski zawijają się do następnego wiersza.
   Minimalna wielkość pojedynczego przycisku jest dostępna w jego ustawieniach.
5. **Ustawienia panelu i okna**, na dole menu PPM, udostępniają minima i
   zaokrąglenia. Wspólne zaokrąglenie to domyślnie **10 px**: jego zmiana działa
   na wszystkie elementy z zaznaczonym dziedziczeniem. Odznacz dziedziczenie,
   aby ustawić własny promień, lub wyłącz zaokrąglenia całkowicie.
6. Rozmiary paneli można określać jako część dostępnego obszaru X/Y. Okna
   zadokowane skalują się z obszarem roboczym; odpięte zapamiętują pozycję i
   rozmiar względem niego. Minima chronią zawartość, a przewijanie obsługuje
   kontenery z większą liczbą elementów. Minimum głównego okna ustaw po wybraniu
   **Całe okno aplikacji**.
7. Przycisk może wywołać funkcję z rejestru (zapis sceny, narzędzia, kamera,
   prymitywy, ustawienia) albo stać się **Wysuwaną listą**. Listy mogą zawierać
   kolejne listy; podmenu otwierają się po najechaniu w prawo. Przypisana funkcja
   listy jest jej pierwszą pozycją **Uruchom**. Nie wykonujemy kodu z pliku HUD.
8. Nowe okna pojawiają się automatycznie w domyślnym menu **Widok**. Można zmienić
   etykietę tego menu, nie tracąc jego powiązania z oknami.
Pasek stanu również jest elementem dokumentu: wybierz go w drzewie, zmień
wysokość/tekst lub ukryj. Dostępne znaczniki: `{status}`, `{objects}`, `{fps}`,
`{limit}`. Po usunięciu można go odtworzyć we właściwościach całego okna.

9. **Zapisz HUD / Ctrl+S** zapisuje `editor_hud.layout` **obok EXE**, niezależnie
   od katalogu roboczego. `engine_editor.exe` odczytuje go przy starcie. Oba EXE
   muszą znajdować się w tym samym katalogu, by używać tego samego pliku.
   Runtime nie nadpisuje projektu przy zamykaniu. Zmiany dokowania w runtime są
   sesyjne; zapisywanie projektu należy do projektanta.

Domyślnie podgląd nie wykonuje funkcji przycisków ani nie manipuluje sceną.
**Interaktywny podgląd** włącza funkcje narzędzi (ruch kamery myszą nadal jest
wyłączony w projektancie, ponieważ PPM służy do edycji HUD-u). Zamknięcie
projektanta z niezapisanymi zmianami wymaga zapisu, odrzucenia lub anulowania.
Przyciski minimalizacji, maksymalizacji i zamknięcia są stałymi kontrolkami
aplikacji poza dokumentem: nie można ich usunąć ani przypisać innej funkcji.

Plik ma wersjonowany format tekstowy `STUDIO_HUD 1`, stabilne identyfikatory,
walidację rodziców/cykli, ograniczenie głębokości do 32 i rozmiaru do 2 MiB.
Zawiera też stan dokowania. Zapis najpierw tworzy plik tymczasowy, następnie
atomowo zastępuje poprzedni; błędne dane nie zastępują dobrego projektu.
Brakujący lub uszkodzony plik powoduje użycie wbudowanego HUD-u. Błąd odczytu
istniejącego pliku trafia do logu; błąd zapisu jest widoczny w projektancie.
Stary `editor_layout.ini` nie jest już automatycznie odczytywany.

### Siatka i testy

Krok siatki jest kwantyzowany wyłącznie według odległości kamery od celu
(zoomu), z tolerancją na błędy zmiennoprzecinkowe obrotu. Obrót kamery zmienia
jedynie obszar pokrycia i projekcję perspektywiczną, nie odstęp linii w świecie.

Testy **nie budują się domyślnie**. Do weryfikacji deweloperskiej użyj
`-DBUILD_TESTING=ON`; CI włącza tę opcję jawnie. W istniejącym katalogu build
z zapamiętanym `BUILD_TESTING=ON` podaj `-DBUILD_TESTING=OFF`.
`hud_tests` sprawdza zapis/odczyt, walidację, zagnieżdżone listy i stały krok
siatki podczas obrotu; Windows `editor_state_tests` renderuje klatki obu trybów
bez GPU, sprawdza zaznaczanie podglądu oraz obecność sceny i własnych okien.

## Ustawienia i bindy

**Ustawienia** są teraz osobnym, pływającym oknem aplikacji. Można je przesuwać,
zmieniać rozmiar i zamykać przyciskiem X, ale nie dokować ani usuwać w projektancie
HUD-u. Wejście jest dostępne w menu Plik oraz na stałym pasku narzędzi
`editor_hud.exe`, również po usunięciu własnego menu projektu.

Zakładki:
- **Podstawowe** — nieaktywna Pomoc i wybór języka z samym Polskim (na razie szary).
- **Grafika** — tryb bez limitu, limit FPS, presety; Vulkan jest szary i nie można
  wybrać innego renderera. Zaawansowane zachowują siatkę, FOV i kolor tła.
  Widać także FPS, czas klatki oraz statystyki przygotowania geometrii na CPU.
- **Bindy** — na samej górze **Przywróć domyślne**. PPM w obszarze tabeli daje
  dokładnie trzy pozycje: **Przypisz funkcję**, **Dodaj binda**, **Usuń binda**.
  Kliknięcie wiersza prawym wybiera go do edycji; na pustym obszarze dostępne
  jest dodawanie, bez ryzyka usunięcia wcześniej wskazanego wiersza.

LPM w polu przypisania uruchamia przechwytywanie klawisza (także z Ctrl, Shift,
Alt, Win) lub przycisku myszy. Klik rozpoczynający przypisywanie nie staje się
samym bindem. Esc anuluje, **Wyczyść przypisanie** usuwa skrót. Konflikty są
odrzucane z komunikatem — nie wywołujemy dwóch funkcji jednym skrótem.
Kombinacje zastrzeżone przez Windows mogą nie trafić do aplikacji.

W **editor_hud.exe** tabela edytuje *domyślne bindy projektu*, zapisane przyciskiem
**Zapisz domyślne w HUD** lub zwykłym zapisem projektu. Projektant zawsze pokazuje
te domyślne, nigdy osobiste nadpisania użytkownika. Żaden bind sceny nie wykonuje
się w projektancie, również w interaktywnym podglądzie. Narzędziowy Ctrl+S służy
nadal do zapisu HUD-u poza oknem ustawień. Przywracanie w projektancie przywraca
fabryczny zestaw podstawowy.

W **engine_editor.exe** domyślne pochodzą z projektu, a własne zmiany zapisują się
w `editor_bindings.cfg` obok EXE. **Przywróć domyślne** przywraca ustawienia autora
HUD-u. Plik nadpisań jest związany z odciskiem zestawu domyślnego — zmiana domyślnych
w projektancie powoduje pominięcie nieaktualnych nadpisań po restarcie edytora.
Skróty nie uruchamiają się podczas wpisywania tekstu, edytowania pola, w menu
kontekstowym ani w otwartych ustawieniach. Stałe gesty orbitowania i kółka kamery
pozostają sterowaniem viewportu, a nie funkcjami jednorazowych bindów.

Zestaw podstawowy zachowuje Ctrl+S, Ctrl+D, Q/W/E, F, N, Delete, strzałki oraz
zoom +/−. Nowe funkcje to m.in. reset transformacji (**Ctrl+R**), przełączanie
siatki (**G**), widoki kamery (**F1–F4**) i ustawienia (**F10**). Można też przypisać
przełączanie konsoli lub bezpośrednie otwarcie zakładki bindów/grafiki, również
jako funkcję przycisku HUD-u.

Plik HUD zapisuje się teraz jako `STUDIO_HUD 2`. Wersja 1 nadal jest odczytywana:
dostaje podstawowe bindy, a dawne wbudowane panele ustawień są migrowane do
nieusuwalnego okna aplikacji. Niestandardowa zawartość takich paneli jest zachowywana.

## Optymalizacje renderera

- Cache przygotowanej geometrii sceny przy nieruchomej kamerze i niezmienionych
  obiektach. Porównywane są rzeczywiste transformacje, typy i ID encji, a także
  kamera, FOV, rozmiar viewportu, zaznaczenie, siatka i tryb gizma; bez polegania
  na liczniku rewizji, który pomijałby edycję przez referencję do encji.
- Gotowe szablony spakowanych prymitywów dla skali jednorodnej — bez ponownego
  liczenia normalnych i konwersji do formatu GPU przy każdym ruchu kamery.
- Jeden import i jedno pakowanie wspólnej geometrii zamiast osobnego pakowania
  debug + ponownego rozpakowania i pakowania HUD-u. Kolejność sekcji pozostaje
  **HUD vertices → 3D vertices → HUD indices → 3D indices**.
- Ponowne użycie pojemności buforów roboczych CPU; limity urządzenia zapamiętane
  zamiast odpytywane w każdej klatce. Aktualizacje deskryptorów tylko po zmianie
  bufora lub jego zakresu, nadal po fence poprzedniej klatki.
- Tabela bindów używa clippera i nie rysuje niewidocznych wierszy.

Statystyka **CPU geometrii** mierzy wyłącznie przygotowanie/odczyt cache, nie
czas GPU ani całą klatkę. FPS zależy także od rozdzielczości, MSAA, sterownika,
prezentacji i HUD-u; nie należy utożsamiać przyspieszenia tego etapu ze wzrostem
FPS. Do porównań buduj **Release** i używaj tej samej sceny oraz rozdzielczości.

Dodatkowe testy opt-in (`BUILD_TESTING=ON`): `bindings_tests` — zapis, walidacja,
konflikty i migracja v1; `geometry_cache_tests` — unieważnianie cache po zmianach;
Windows `editor_state_tests` — niedokowalne ustawienia, przechwytywanie skrótów,
konflikty, przywracanie, nieaktywne bindy projektanta i remapowanie w runtime.
