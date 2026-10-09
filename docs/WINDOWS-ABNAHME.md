# Offene Windows-Abnahme

**Für Version 1.0.1 wurde diese Prüfliste noch nicht unter Windows ausgeführt.** Der Start von Version 1.0.0 ist beim Anwender mit 0xC000007B fehlgeschlagen. Der Hotfix korrigiert den nachgewiesenen Fehler in der PE-Startkonfiguration, ist aber noch nativ zu bestätigen. Sie trennt echte Kern-/Buildtests von den noch ausstehenden GUI- und Integrationsprüfungen. Ziel: Windows x64 mit normalem Benutzerkonto, ohne nachinstallierte Laufzeiten und ohne MySQL.

| Prüfung | Erwartung | Status |
|---|---|---|
| ZIP entpacken / EXE starten | Bit-Line-Fenster ohne fehlende DLL | Offen |
| Demo laden | 2 Datenbanken; 8 Tabellen; 7.230 Einträge; 2 Views, 1 Routine als DDL | Offen |
| shop_demo → categories | 14 Zeilen und 7 Spalten | Offen |
| analytics → events | 5.000 Zeilen, 100 pro Seite | Offen |
| Letzte / direkte Seite | Zeilen 4.901–5.000 auf Seite 50 | Offen |
| event_type ist gleich checkout | 1.250 Treffer | Offen |
| Filter zurücksetzen | Wieder alle 5.000 Einträge | Offen |
| NULL-Filter / settings-Spaltenliste | Fehlend vs. NULL unterscheidbar | Offen |
| Struktur / Original-SQL / Hinweise | Tabellen-DDL und Definitionen lesbar | Offen |
| Doppelklick / SQL-Tupel | Details lesbar / bytegenauer Tupelausschnitt | Offen |
| CSV Tabelle / Filter | 5.000 bzw. 1.250 Datenzeilen plus Kopf | Offen |
| Abbruch Export / vorhandenes Ziel | Bestehende Datei bleibt erhalten | Offen |
| Dump neu öffnen / Neu indexieren | Cache wiederverwendet / explizit neu aufgebaut | Offen |
| Zweite Instanz mit gleichem Dump | Klare Cache-Sperrmeldung | Offen |
| Drag-and-drop / Unicodepfad / langer Pfad | Datei ohne Namensverlust lesbar | Offen |
| 100 %, 150 %, 200 % DPI / Resize | Bedienbare Controls, keine wesentlichen Überlappungen | Offen |
| Strg+O / Strg+F / Strg+C / Tastaturnavigation | Erwartete Aktionen, Fokus sichtbar | Offen |
| Schließen während Import | Abbruchabfrage, kein weiterlaufender Prozess | Offen |
| Fehlerhafte / abgeschnittene SQL-Datei | Verständlicher Fehler, kein fertiger Teilcache | Offen |
| Eigener realer mysqldump inkl. Views/Routinen | Grenzen und Importhinweise akzeptabel | Offen |
| Realer >4-GiB-Dump / RAM-/I/O-Messung | Systembezogene Messung, keine Ganzdatei-RAM-Ladung | Offen |
| NTFS, externer Datenträger, voller Cache-Datenträger | Korrekte I/O-Fehler, Original unverändert | Offen |
| Signatur / Reputation / interne Sicherheitsprüfung | Der Build ist nicht signiert; Freigabe separat | Offen |

Für Fehlerberichte reichen zunächst Windows-Version, genaue Aktion, Fehlermeldung und ein kleiner anonymisierter SQL-Ausschnitt. Keine Passwörter oder Produktivkundendaten ungefragt weitergeben. Bis zur eigenen Abnahme mit einer Dump-Kopie arbeiten.
