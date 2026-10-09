# Bit-Line Dump Browser 1.0.1 – Start-Hotfix und Fehleranalyse

Stand: 09.10.2026. **Buildfehler statisch korrigiert; nativer Windows-Start noch nicht bestätigt.**

## Befund am tatsächlich ausgelieferten Build 1.0.0

Die untersuchte Datei stammt aus dem zuvor bereitgestellten Windows-ZIP. Sie ließ sich aus dem ausgelieferten Quellcode byteidentisch reproduzieren.

| Merkmal | Version 1.0.0 | Version 1.0.1 |
|---|---|---|
| Dateiformat | PE32+ | PE32+ |
| COFF Machine | 0x8664 / AMD64 | 0x8664 / AMD64 |
| OS-Version im PE-Header | 10.0 | 6.0 |
| Subsystem | Windows GUI | Windows GUI |
| Subsystem-Version im PE-Header | 10.0 | 6.0 |
| Load Configuration Directory | RVA 0 / Größe 0 | RVA 0 / Größe 0 |
| Importierte DLLs | 8 Windows-System-DLLs | Dieselben 8 DLLs, dieselben Funktionen |
| Anwendungsmanifest | amd64; Common Controls arch=* | amd64; Common Controls arch=* |
| ASLR / High-Entropy ASLR / DEP | gesetzt | unverändert gesetzt |
| Code-Signatur | keine | keine |
| Dateigröße | 382.976 Byte | 382.976 Byte |

**Kein Architektur-Mix im Paket gefunden.** Die EXE ist x64, und im Windows-Paket liegen keine zusätzlichen DLL-Dateien. Die Prüfung untersucht das ausgelieferte Paket, nicht die auf dem Rechner des Anwenders tatsächlich geladenen Module; einen zusätzlichen lokalen Konflikt kann sie daher nicht ausschließen.

Die direkten Imports sind: `kernel32.dll`, `user32.dll`, `gdi32.dll`, `comctl32.dll`, `comdlg32.dll`, `shell32.dll`, `uxtheme.dll` und `msvcrt.dll`. Es gibt keine Imports auf `VCRUNTIME140.dll`, `MSVCP140.dll`, `libgcc_s_*.dll` oder `libwinpthread-1.dll`. Für diesen Fix wird kein Redistributable nachinstalliert und keine DLL ausgetauscht.

## Konkreter Buildfehler und Einordnung

Der bisherige Linkeraufruf kombinierte `/subsystem:windows,10.00` mit einem eigenen Programmeinstieg und `/nodefaultlib`. Dieser schlanke Build stellt keine Load-Configuration-Struktur mit GS-Security-Cookie bereit.

Für Subsystem-Versionen ab 6.3 ist die Ablehnung dieser Konstellation mit `STATUS_INVALID_IMAGE_FORMAT` / `0xC000007B` in den unten genannten Primärquellen dokumentiert. Sie passt zum gemeldeten Startfehler und ist die **sehr wahrscheinliche Ursache**. Eine Windows-Laufzeitreproduktion oder ein Loader-Trace vom betroffenen Rechner liegt nicht vor; die konkrete Kombination im ausgelieferten PE wurde jedoch nachgewiesen.

Die alte statische Prüfung kontrollierte Architektur, Imports, Ressourcen und einige Sicherheitsflags, aber nicht diese Anforderung an die Loader-Metadaten. Dass sie den Build akzeptierte, bewies daher keine Startfähigkeit. Diese Lücke ist jetzt geschlossen.

## Korrektur

Neu verlinkt mit:

```text
/machine:x64 /subsystem:windows,6.00 /osversion:6.00
```

6.00 ist der dokumentierte Standardwert für das x64-Windows-Desktop-Subsystem. Damit verwendet dieser Build ohne eigene GS-Load-Configuration nicht mehr den unpassenden höheren Loader-Vertrag. Das ist **keine Umstellung auf 32 Bit**, kein Herabsetzen von Windows-Systemeinstellungen und keine Installation alter DLLs.

Die tatsächliche Zielplattform bleibt **Windows 10 ab Version 1703 bzw. Windows 11, x64**, weil die Anwendung entsprechende APIs verwendet. Die Versionsfelder im PE-Header sind kein Versprechen, dass die Anwendung auf Windows Vista/7/8 lauffähig ist. Manifest, DPI-APIs und Funktionsumfang bleiben unverändert.

Es wird auch keine neu hinzugefügte Stack-Canary-Absicherung behauptet: Wie im Original ist kein eigener GS-Security-Cookie implementiert. ASLR, High-Entropy ASLR und DEP bleiben aktiviert.

Außer diesen zwei Headerbytes wurden sieben Versionsbytes auf 1.0.1 aktualisiert. Der Vergleich der fertigen EXEs zeigt **genau neun unterschiedliche Dateibytes**. Die komplette `.text`-Sektion mit dem ausführbaren Maschinencode ist byteidentisch, ebenso die importierten DLL- und Funktionslisten. Parser, Datenhandling und GUI-Logik wurden in diesem Hotfix nicht verändert.

## Tatsächlich erneut geprüft

| Prüfung | Ergebnis |
|---|---|
| Windows-x64-Cross-Build mit Clang/LLD 17 | erfolgreich |
| Zweiter vollständiger Build | byteidentische EXE |
| Neue PE-/Loader-Prüfung | korrigierter Build akzeptiert |
| Ausgelieferte Original-EXE gegen neue Prüfung | mit Hinweis auf 0xC000007B abgewiesen |
| Neue PE-Regressionssuite | 12 Testfälle bestanden |
| Parser-/Cache-/Filter-/Exportregressionen | 146 Assertions bestanden, ASan + UBSan |
| Sparse-Dateitest für >4-GiB-Adressierung | 21 Assertions bestanden, ASan + UBSan |
| Demo vollständig indexiert und dekodiert | 7.230 Datensätze; keine Importhinweise |
| Maschinencode und Imports vor/nach Hotfix | identisch |
| Nativer Windows-Start und GUI-Bedienung | **nicht durchgeführt** |

Die zwölf neuen Testfälle prüfen den gültigen Build sowie absichtlich veränderte Kopien: die alte 10.0-Konstellation, Subsystem 6.3 ohne Load Configuration, eine unerwartete OS-Header-Version, falsche Machine-/PE32-Kennung, falsches Subsystem, fehlende DEP-/ASLR-Flags, falsche Architekturen im Anwendungs-/Abhängigkeitsmanifest und eine unerwartete DLL-Abhängigkeit. Diese Tests sind **statische Regressionstests, keine Simulation des gesamten Windows-Loaders**.

Die 146 und 21 Kernprüfungen sind Assertions innerhalb zweier Programme, nicht ebenso viele unabhängige Testfälle. Der große Millionenzeilen-Benchmark aus 1.0.0 wurde für diesen reinen Start-Hotfix nicht erneut ausgeführt. Sein alter Bericht ist ausdrücklich als historischer Bericht unter `docs/archive` erhalten.

## Anwenden

Das ZIP **in einen neuen, leeren Ordner entpacken** und dort `BitLineDumpBrowser.exe` starten. Nicht versehentlich die alte EXE über eine vorhandene Verknüpfung öffnen. Der alte Dump-Cache muss für diesen Start-Fix nicht gelöscht werden; das Cacheformat wurde nicht verändert. Anschließend zuerst die Demo laden.

Falls der Start weiterhin scheitert, sind der neue Fehlercode, Windows-Version einschließlich Build und der Systemtyp (x64, x86 oder ARM64) die nächsten nötigen Informationen. Keine beliebigen DLLs herunterladen, keine Windows-System-DLLs in den Programmordner kopieren und keinen Virenschutz pauschal deaktivieren.

## Dateiidentität

```text
Original 1.0.0 SHA-256:
86b6d72b782d385cc1735eb15d9d0c72b3af3bc2195c3d8ded69e22e11fcd20e

Hotfix 1.0.1 SHA-256:
07ff6ab220909be6530f2984eb88618f38351de249409eed91551ef167120f51
```

Die SHA-256-Prüfsumme vergleicht Dateiinhalte; sie ersetzt keine Herausgebersignatur.

Rohbelege im Quellpaket und Windows-Paket: `docs/test-data/pe-report-1.0.1.json`, `hotfix-comparison.json`, `pe-regression-1.0.1.log`, `core-1.0.1.log` und `build-1.0.1.log`.

## Primärquellen zur Fehlerursache

Eigene Dateibefunde und Testergebnisse stammen aus den lokal ausgeführten Prüfungen. Die allgemeine Einordnung des Windows-Loaders stützt sich auf:

1. Microsoft: NTSTATUS Values – 0xC000007B = STATUS_INVALID_IMAGE_FORMAT; 0xC0000135 = STATUS_DLL_NOT_FOUND. https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/596a1078-e883-4972-9bbc-49e60bebca55
2. Microsoft: /SUBSYSTEM – Bedeutung der Versionsfelder und Standard 6.00 für x86/x64 WINDOWS. https://learn.microsoft.com/en-us/cpp/build/reference/subsystem-specify-subsystem?view=msvc-170
3. Microsoft: IMAGE_LOAD_CONFIG_DIRECTORY64 – unter anderem das SecurityCookie-Feld. https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_load_config_directory64
4. MinGW-w64-Entwickler-Mailingliste: „Images created with --subsystem console:10.0 fail to run“, März/April 2026; beschreibt auch ein minimales MSVC-Beispiel mit /nodefaultlib. https://sourceforge.net/p/mingw-w64/mailman/mingw-w64-public/thread/20260330094622.q6kjmq53z56krvbk%40pali/
5. LLVM-MinGW-Projekt, Issue #511: SecurityCookie und MajorSubsystemVersion 10. https://github.com/mstorsjo/llvm-mingw/issues/511

Kein Windows-Laufzeittest, kein Security-Audit und keine Produktionsfreigabe werden aus diesen Quellen oder statischen Prüfungen abgeleitet.
