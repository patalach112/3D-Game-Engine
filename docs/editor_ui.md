# Edytor: panele, pasek tytułu i konsola

## Układ

- Hierarchia po lewej, inspektor po prawej, scena w środku, konsola na dole.
- Przeciągnij zakładkę lub nagłówek, aby odpiąć panel albo zadokować go przy
  krawędzi/innej zakładce. Separatory zmieniają rozmiary paneli.
- Zasoby, Notatki i Renderer są dodatkowymi zakładkami dolnego obszaru.
- Menu **Widok** przywraca zamknięte panele; **Przywróć układ** resetuje docking.
- Układ zapisuje się w `editor_layout.ini` w katalogu roboczym. Nie używamy
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
