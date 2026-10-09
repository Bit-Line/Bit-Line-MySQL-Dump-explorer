# Bit-Line Dump Browser · 1.0.1

Nativer Windows-x64-Browser für lokale MySQL-Dumps. Kein Datenbankserver, kein lokaler Webserver, kein .NET, Python oder Java zum Ausführen. Die EXE verwendet ausschließlich DLLs des Windows-Betriebssystems. C17 / Win32; eigenes, schreibgeschütztes Streaming- und Positionsindexformat.

**Auslieferungsstatus:** Start-Hotfix für den 0xC000007B-Fehler in 1.0.0. Die unpassende PE-Subsystem-Konfiguration wurde korrigiert; siehe `docs/TESTBERICHT.md`. Build und Kernregressionen wurden erneut geprüft. Der native Start und die Windows-Oberfläche wurden in der verfügbaren Umgebung **nicht ausgeführt**. Die EXE ist nicht codesigniert. Die Prüfliste für die noch offene Windows-Abnahme liegt in `docs/WINDOWS-ABNAHME.md`.

## Start für Anwender

Das Windows-Paket vollständig in einen neuen, leeren Ordner entpacken und `BitLineDumpBrowser.exe` starten. `Demo laden` öffnet den mitgelieferten Beispieldump. Eigene `.sql`-Dateien über `Dump öffnen …` oder Drag-and-drop öffnen. Zielplattform: Windows 10 ab Version 1703 bzw. Windows 11, x64. Keine zusätzliche Installation; kein Administratorrecht vorgesehen. Keine native ARM64- oder 32-Bit-Ausgabe.

Der Dump bleibt unverändert. Der Cache liegt unter `%LOCALAPPDATA%\Bit-Line\DumpBrowser\Cache`. Die EXE ist ohne Installation nutzbar, aber **nicht vollständig USB-portabel**: Der Cache wird im Benutzerprofil angelegt. Die Originaldatei muss für das Lesen erhalten bleiben.

## Funktionen

Datenbank-/Tabellenbaum mit Namenfilter; 100 Datensätze pro Seite mit direkter Seitennummer; Struktur und Original-DDL; Datensatzdetails; rohe SQL-Tupel speichern; Filter pro Spalte oder über alle Spalten; `enthält`, `ist gleich`, `ist NULL`, `ist nicht NULL`; CSV für die gesamte aktuelle Tabelle oder das gefilterte Ergebnis; UTF-8, Windows-1252 und Latin-1; Fortschritt und Abbruch für lange Dateioperationen; wiederverwendbarer Cache; Bit-Line-Branding und eingebettete Icons.

Es handelt sich um einen **Dump-Browser, nicht um einen MySQL-Interpreter**. Kein SQL-Editor, keine Schreibfunktionen, keine globale Sortierung, keine JOINs, kein Anwenden von UPDATE/DELETE/REPLACE-Semantik. Numerische Filter vergleichen den angezeigten Literaltext, nicht MySQL-Datentypwerte. Siehe `docs/KOMPATIBILITAET.md`.

## Windows-EXE selbst bauen

Entwicklungswerkzeuge: Python 3.10 oder neuer und LLVM mit `clang` und `lld-link`, jeweils auf PATH. Das sind ausschließlich Build-Voraussetzungen, keine Laufzeitabhängigkeiten für Nutzer. Die bereits enthaltenen Assets benötigen keine Bildbibliothek. Keine Downloads während des Builds.

```console
python tools/build.py
python tools/test_pe.py
```

Ausgabe: `build/windows/BitLineDumpBrowser.exe`. Alternative Toolpfade:

```console
python tools/build.py --clang /pfad/zu/clang --link /pfad/zu/lld-link
```

Der Build erstellt kleine Importbibliotheken für dokumentierte Windows-Funktionen, kompiliert die drei C-Module und den x64-Stack-Probe-Helfer, erzeugt PE-Ressourcen und verlinkt die grafische Anwendung. Der Build führt die PE-Prüfung zwingend aus; die neue Regressionssuite lehnt unter anderem die fehlerhafte 10.0-Konstellation ab. Die PE-Subsystem- und OS-Header-Version 6.00 ist vom tatsächlichen Windows-API-Minimum 10/1703 zu unterscheiden. Ein Windows SDK wird für diesen Build nicht benötigt. Die minimalen Win32-ABI-Deklarationen stehen in `src/winmini.h`. Getestete Build-Werkzeuge siehe Testbericht.

## Kern testen / Benchmark reproduzieren

Unter Linux mit Python und GCC:

```console
python tools/test.py
python tools/generate_dump.py build/million.sql --rows 1000000
gcc -std=c17 -D_POSIX_C_SOURCE=200809L -O2 src/core.c src/platform.c tests/benchmark.c -o build/benchmark
./build/benchmark build/million.sql build/benchmark-cache
```

Der Sparse-Dateitest prüft 64-Bit-Adressierung jenseits 4 GiB, ohne mehrere GiB lesen zu müssen. Er ersetzt ausdrücklich keinen kompletten Import eines mehrere GiB großen Dumps. Der Benchmark erzeugt bzw. liest reale 148.889.120 Bytes und 1.000.000 Zeilen, nicht nur einen Index-Mock.

## Projektstruktur

`src/core.c`: Streaming-Parser, Schema, Index, Filter, Exporte. `src/platform.c`: Datei- und Zeichensatzbrücke. `src/app.c`: native GUI und Hintergrundjobs. `src/winmini.h`: Win32-ABI. `tools`: reproduzierbarer Build, Tests und Datengeneratoren. `tests`: Core-Regressionen, Sparse-Adressierung und Benchmark. `assets`: eingebettete Originalmarke, App- und Navigationsicons. `samples`: vollständig synthetischer Demo-Dump. `docs`: Architektur, Formatgrenzen, Testbericht und offene Abnahme.

## Sicherheit / Lizenz

SQL wird nie ausgeführt. Es gibt keinen Netzwerkclient, Telemetrie-Code, Update-Downloader oder eingebetteten Browser. Ausgaben und Cache bleiben lokale Dateien. CSV ist ein Ansichts-/Analyseexport, kein verlustfreies MySQL-Backup. Aufbewahrung und Berechtigungen des Dumps, Cache und Exports liegen beim Anwender. Sensitive Dateien nicht ohne ausdrückliche Freigabe an öffentliche Scanner senden.

Code: MIT-Lizenz (`LICENSE.txt`). Bit-Line-Logo und Markenrechte sind davon ausgenommen (`THIRD_PARTY_NOTICES.txt`).
