> **Historischer Bericht zu 1.0.0, nicht der Freigabebericht des Hotfixes.** Nach Auslieferung wurde ein Windows-Startfehler 0xC000007B gemeldet. Die damalige statische Prüfung hatte eine unpassende Loader-Konfiguration übersehen. Maßgeblich ist jetzt `../TESTBERICHT.md`. Die ursprünglichen Benchmarkzahlen wurden für 1.0.1 nicht erneut erhoben.

# Bit-Line Dump Browser 1.0.0 – Test- und Buildbericht

Stand: 09.10.2026. Auslieferungsstatus: **Windows-Erstbuild; GUI-Abnahme offen.**

## Tatsächlich durchgeführt

| Prüfung | Ergebnis |
|---|---|
| Native Windows-x64-EXE cross-kompilieren | Erfolgreich, Clang/LLD 17.0.0 |
| Strenge Compilerwarnungen und Win32-x64-Strukturgrößen | Build erfolgreich; 19 statische ABI-Größenprüfungen |
| PE-Format / Einstiegspunkt / Ressourcen | PE32+ x64 GUI; 6 Sektionen; 32 Ressourcen; Manifest, Version, Icons und Wordmark enthalten |
| Importbibliotheken | Genau 8 Windows-System-DLLs; keine zusätzlichen Laufzeitpakete |
| ASLR / High-Entropy ASLR / DEP | Im PE gesetzt; keine Behauptung eines vollständigen Sicherheitsaudits |
| Parser-, Filter-, Cache- und Exportregressionen | **146 Assertions bestanden**, GCC 14.2.0, AddressSanitizer + UndefinedBehaviorSanitizer; keine gemeldeten Fehler |
| 64-Bit-Datei-/Indexadressierung | **21 Assertions bestanden**, ASan + UBSan; Quelldateiposition 4.294.968.530, Indexposition 4.800.000.000 Byte |
| Mitgelieferter Demo-Dump | Alle **7.230 Datensätze** indexiert und dekodiert; 2 Datenbanken, 8 Tabellen, 2 Views, 1 Routine; keine Importhinweise |
| Großer erweiterter INSERT | **1.000.000 Datensätze**, 148.889.120 echte Dateibytes; Index, spätere Seite, Filter, CSV und Cache-Neuöffnung erfolgreich |
| Grafische Oberfläche unter Windows starten / bedienen | **Nicht durchgeführt** – kein Windows-/Wine-Laufzeitsystem in der Buildumgebung |
| Code-Signatur | **Nicht vorhanden** |

Die 167 Core-/Adressierungsprüfungen sind Assertions innerhalb zweier Testprogramme, nicht 167 unabhängige Testfälle. Die Demoprüfung durchläuft zusätzlich jeden Datensatz. Der >4-GiB-Test verwendet Sparse-Dateien und kontrollierte Indexpositionen; er prüft die Adressierung, **nicht den vollständigen Scan eines >4-GiB-Dumps**.

## Abgedeckte Parser- und Exportfälle

Mehrere Datenbanken, qualifizierte/escaped Namen, CREATE-Definitionen, erweiterte INSERTs, exakte große Ganzzahlen und Dezimalliterale, umsortierte Spaltenlisten, ausgelassene Spalten vs. NULL, Quote-/Backslash-Escapes, Unicode, Binär-/Hex-/Bitwerte, NO_BACKSLASH_ESCAPES einschließlich Wiederherstellung, Kommentare innerhalb von Feldern, DELIMITER-Routinen ohne Ausführung ihrer INSERTs, DDL-Vorschau, ungültige/abgeschnittene Eingaben, Filter, CSV, Cache-Neuöffnung und beschädigtes Manifest, Abbruch sowie bytegenauer Rohtupel-Export mit Erhalt eines bestehenden Ziels bei Abbruch.

## Synthetischer Benchmark – keine Windows-GUI-Messung

Linux-x64-Container, GCC 14.2.0, `-O2`, **ohne Sanitizer**; lokales Dateisystem, warmer OS-Dateicache möglich. Ein erweitertes INSERT mit 1.000.000 Zeilen, sechs Spalten, UTF-8, Escapes, Binär- und Dezimalwerten. Kein unabhängiger Hardwarebenchmark; keine Garantie für reale Daten oder Windows-Laufzeiten.

| Messgröße | Beobachtung dieses Laufs |
|---|---:|
| SQL-Datei | 148,889,120 Byte |
| Positionsindex | 16,000,000 Byte = ca. 15,26 MiB |
| Indexaufbau inkl. Dateivorbereitung | 487 ms |
| 100 Datensätze ab Indexposition 900.000 dekodieren | 1 ms |
| Exakter Spaltenfilter über 1 Mio. Zeilen | 714 ms |
| Filtertreffer | 10,000 |
| CSV der 10.000 Treffer | 21 ms |
| Cache neu öffnen | 1 ms |
| Maximale RSS des headless Benchmarkprozesses laut `/usr/bin/time -v` | 1.924 KiB |

Die Zeitauflösung beträgt Millisekunden; sehr kleine Messwerte sind nicht präzise genug für allgemeine Leistungsversprechen. RSS betrifft nur den headless Linux-Prozess in diesem Test, **nicht** Windows, die GUI, extrem breite Schemata oder große Einzelwerte. Die maximalen 64-MiB-Einzeltupel können beim Dekodieren erheblich mehr Arbeitsspeicher benötigen. Die garantierte Formatgröße des Positionsindexes (16 Byte pro Tupel) ist von diesen Laufzeitbeobachtungen zu unterscheiden.

## EXE-Identität

- Dateiname: `BitLineDumpBrowser.exe`
- Größe: 382,976 Byte
- SHA-256: `86b6d72b782d385cc1735eb15d9d0c72b3af3bc2195c3d8ded69e22e11fcd20e`
- Ziel: Windows x64, Subsystem 10.0; verwendete DPI-APIs ab Windows 10 Version 1703.
- Import-DLLs: kernel32, user32, gdi32, comctl32, comdlg32, shell32, uxtheme, msvcrt.
- Signiert: nein. Die Prüfsumme dient dem Vergleich von Dateiinhalten; sie ersetzt keine Herausgebersignatur.

## Offen / nicht behauptet

Windows-GUI-Funktion, Layout bei unterschiedlichen DPI-/Monitor-Konfigurationen, reale Dateidialoge, Clipboard, Drag-and-drop, Windows-Dateisperren, Fehlerdialoge, langfristige Stabilität, echte Produktions-Dumpvarianten und Vollimporte mehrerer GiB/TiB wurden hier nicht end-to-end überprüft. Ebenfalls kein professionelles Fuzzing, Malwarezertifikat, Code-Signing, Windows-Zertifizierung oder vollständiger Security-Audit. Der Parser deckt nicht sämtliche MySQL-/MariaDB-Semantik ab.

Vor produktivem Einsatz die Prüfliste `WINDOWS-ABNAHME.md` durchführen und zunächst mit einer Kopie eines Dumps arbeiten. Die Anwendung ist schreibgeschützt; Exporte und Cache sind unverschlüsselte lokale Dateien.

## Reproduzierbarkeit (historisch)

Im Quellpaket liegen `tools/build.py`, `tools/test.py`, `tools/verify_pe.py`, die Tests, beide Datengeneratoren und die Rohberichte unter `docs/test-data`. Sie benötigen für die Ausführung Entwicklungswerkzeuge; Nutzer der fertigen EXE nicht. Der freigegebene Build wurde nach den letzten Codeänderungen erneut erzeugt und statisch geprüft.
