# LiDAR Escape

# Opis
Projekt to gra typu escape room w stylu liminal-horror, w której gracz widzi świat tylko dzięki skanowaniu LiDAR, rozwiązuje puzzle i unika wrogów w ciemności.

# Funkcje
Skanowanie i widoczność

 1) Local scan (LPM / przytrzymanie LPM): ujawnia mały obszar przed graczem, precyzyjne sprawdzanie, nie alarmuje wrogów, o ile nie skanujesz bezpośrednio ich sylwetki

 2) Area scan (PPM / przytrzymanie PPM): mapuje większą strefę, szybciej zużywa baterię, zwiększa szansę wykrycia

 3) Trwałość: punkty stopniowo znikają, więc gracz musi ciągle skanować

Środowisko i zagrożenia

 1) Niewidzialne pułapki (dziury, przepaście, spadki), upadek = natychmiastowa śmierć

 2) Elementy prowadzące: kolorowe strzałki na ścianach kierujące do celów w trudnych fragmentach

 3) Dynamiczne pokoje: sekcje labiryntu + proste interakcje i puzzle

Puzzle

 1) Szukanie przełączników/przycisków

 2) Kody: symbole rozsiane po poziomie do odczytu kodu (opcjonalnie)

 3) Zbieranie przedmiotów: baterie, wskazówki i ukryte przedmioty wykrywalne skanem

Wrogowie i napięcie

 1) Stalker: humanoid, glitch/zakłócenia ekranu przy bliskości, skanowanie może go alarmować i uruchamia pościg, kontakt = śmierć

 2) Pasożyty (rzadkie zdarzenie): wskakują na ekran i drenują baterię; trzeba "otrząsnąć" myszką

 3) Audio: beep przy interaktywnych obiektach, kroki/oddech wrogów, szum/ładowanie skanera

 4) Stealth: wrogowie “słyszą” skan, kucanie i wolniejszy ruch zmniejszają wykrywalność

Systemy gry

 1) Bateria: ograniczony zasób, doładowanie za pomocą przedmiotów

 2) Warstwa dźwiękowa: oddech/dźwięk serca podbijające stres

 3) Kolory punktów: np. białe = ściany, czerwone = wrogi

 4) Poziomy/sektory: rosnąca trudność i złożoność

# Harmonogram
04.03.2026 – 17.03.2026
Do oddania: repo + struktura projektu, scena testowa (korytarz/pokój), podstawowy kontroler gracza, prototyp LiDAR, podstawowe UI baterii (placeholder)

18.03.2026 – 31.03.2026
Do oddania: dwa tryby skanu (Local/Area), zanikanie punktów w czasie, zużycie baterii i prosty system ładowania, pierwsze “niewidzialne” pułapki (dziura/przepaść)

01.04.2026 – 14.04.2026
Do oddania: 1 mini-sektor (3–5 pomieszczeń), strzałki prowadzące (guide elements), interakcje (przycisk/przełącznik), prosta zagadka typu “znajdź i aktywuj”

15.04.2026 – 28.04.2026
Do oddania: system kodu do drzwi (panel + wprowadzanie), rozrzucone symbole do kodu, przedmioty (bateria/papiery ze wskazówkami), audio podstawowe (beep interakcji + ambient), pierwsza sensowna pętla rozgrywki (wejście → puzzle → wyjście)

29.04.2026 – 12.05.2026
Do oddania: wróg Stalker (patrol + wykrywanie), reakcja na skan (alert), pościg i warunek przegranej, efekty napięcia (glitch/oddech), balans: jak szybko znika chmura punktów vs tempo skanu

13.05.2026 – 26.05.2026
Do oddania: wrogi pasożyty (drenaż baterii i mechanika zrzucania myszką), dopracowanie sektorów (min. 1 pełny poziom do przejścia), optymalizacja (FPS + liczba punktów), testy błędów

27.05.2026 – 31.05.2026 (final)
Do oddania: wersja finalna “bez błędów krytycznych”, przygotowana prezentacja 10–15 min + demo przejścia fragmentu gry

# Do użycia
SFML, OpenGL, C++, Github do kontroli wersji

# Przykłady podobnych programów
Główna inspiracja: https://www.roblox.com/games/73643462799363/LiDAR 

Liminal horrors: https://store.steampowered.com/app/1825160/The_Backrooms/

https://store.steampowered.com/app/2663530/POOLS/

Użycie baterii: https://store.steampowered.com/app/319510/Five_Nights_at_Freddys/

Wróg stalker + liminal space: https://www.curseforge.com/minecraft/mc-mods/wonderland-jar

Pasożyty: https://www.roblox.com/games/12411473842/Pressure

https://pressure.fandom.com/wiki/Parasite
