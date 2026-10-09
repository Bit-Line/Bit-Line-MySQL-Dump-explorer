# Formatunterstützung und klare Grenzen

## Vorgesehene Eingaben

Unkomprimierte `.sql`-Textdateien im typischen MySQL-VALUES-Dumpstil. Erkannt werden `CREATE DATABASE`, `USE`, qualifizierte Tabellennamen, einfache `CREATE TABLE`-Definitionen, Spalten und deren Originaldefinitionen, sowie `INSERT [IGNORE] INTO ... VALUES` und `REPLACE ... VALUES`. Ausgewertete Daten werden nicht benötigt: Der Browser liest die im Dump vorhandenen Literale.

Erweiterte INSERTs mit sehr vielen Tupeln, Zeilenumbrüchen und Kommentaren können gestreamt werden. Unterschiedliche explizite Spaltenlisten werden je INSERT zugeordnet. Fehlt `CREATE TABLE`, können Spalten aus der INSERT-Liste oder generische Namen entstehen; eine spätere komplexe Schemaänderung wird nicht nachgebildet. Ein Dump ohne Datenbankangabe erhält einen Ersatznamensraum.

Unterstützte Literalformen: NULL, vorzeichenbehaftete oder exponentielle Zahlen als exakter Text, einfache/doppelte SQL-String-Literale, doppelte Quote-Escapes, MySQL-Backslash-Escapes, Zeichensatzintroducer wie `_utf8mb4`, `_binary`, Hexwerte `0x...` / `X'...'` und Bit-Literale. Binärspalten bleiben in Hex-/Bitdarstellung. BIGINT/DECIMAL werden nicht in Fließkommazahlen umgerechnet.

Die Einstellung `NO_BACKSLASH_ESCAPES` wird bei einfachen wörtlichen SQL_MODE-Zuweisungen und den üblichen mysqldump-Speicher-/Wiederherstellungsformen verfolgt. Komplexe/dynamische SQL_MODE-Ausdrücke und ANSI_QUOTES sind nicht unterstützt. Eine Zeichensatzintroducer-Auswertung im Sinne einer MySQL-Zeichensatzkonvertierung erfolgt nicht; die Anzeige verwendet die gewählte globale Codierung.

Die Anzeige bietet UTF-8, Windows-1252 und ISO-8859-1. Ungültige UTF-8-Bytes werden im Text durch Ersatzzeichen dargestellt. Die Originalbytes bleiben im Dump/Rohtupel erhalten. Keine UTF-16-/UTF-32-Dumps, keine automatische Konvertierung zwischen beliebigen MySQL-Zeichensätzen. Die Schema-Syntax wird byteorientiert gelesen; ein falsch gewählter Zeichensatz ändert die Anzeige, nicht die Indexpositionen.

## SQL ist Daten, kein Programm

Alle VALUES-Tupel erscheinen in Dump-Reihenfolge, auch Duplikate oder REPLACE-Zeilen. Es findet keine Prüfung oder Anwendung von Primär-/Fremdschlüsseln, UNIQUE, DEFAULT, GENERATED, AUTO_INCREMENT oder CHECK statt. Eine nicht mitgelieferte Spalte ist ausdrücklich als „nicht im INSERT“ markiert und nicht als NULL oder errechneter Default.

SQL-Ausdrücke werden als `SQL:` angezeigt und nicht ausgeführt. UPDATE, DELETE, ALTER, DROP und TRUNCATE werden nicht zur Berechnung eines Datenbank-Endzustands angewandt. Daher ist das Tool kein geeigneter Interpreter für Migrations-, Änderungs-, Wiederherstellungs- oder Replikationsskripte.

Einfache Views und Routinen können als DDL-Vorschau im Baum erscheinen. `DELIMITER` wird zum sicheren Überspringen/Erfassen berücksichtigt. Views, Routinen und Trigger erzeugen keine durch die Anwendung berechneten Daten. MySQL-/MariaDB-Code in Versionskommentaren wird – außer den unterstützten SQL_MODE-Formen – nicht interpretiert. Versionskommentar-basierte Views/Routinen sind daher nicht vollständig dargestellt. Es gibt keine Garantie für alle MySQL-/MariaDB-Exportvarianten; reale eigene Dumps gehören zur Abnahme.

Nicht unterstützt: komprimierte ZIP/GZIP/Zstd-Dateien direkt, `LOAD DATA`, `INSERT SELECT`, `INSERT SET`, beliebige PARTITION-Insertvarianten, Daten aus OUTFILE-Dateipaketen, Binlogs, separate Schema-/Daten-Dateien als automatisch zusammengeführtes Projekt, SQL-Abfragen, JOINs, Bearbeitung oder globale Sortierung. Syntaktisch nicht sicher lesbare INSERTs brechen mit Fehlerhinweis ab; andere Anweisungen können ignoriert bzw. protokolliert werden. Importhinweise lesen.

## Filtersemantik

`enthält` und `ist gleich` arbeiten auf Text und beachten Groß-/Kleinschreibung. `1`, `01` und `1.0` sind beim Gleichheitsfilter unterschiedliche Literaltexte. Keine SQL-Wildcards, Regex, MySQL-Collations, numerischen Vergleiche oder kombinierbaren Filterchips. Über „Alle Spalten“ genügt eine passende Spalte. Bei „ist nicht NULL“ genügt dort entsprechend eine vorhandene, nicht-NULL-Spalte; fehlende INSERT-Spalten zählen nicht als NULL. Namenfilter im linken Baum ignoriert ASCII-Groß-/Kleinschreibung, nicht sämtliche Unicode-Schreibvarianten.

## Größenlimits

Dateipositionen und Zeilenzahlen werden intern in 64 Bit geführt. Der Tupellängenwert im Index ist 32 Bit: Ein einzelnes Tupel muss kleiner als 4 GiB sein. Maximal 4.096 Spalten je Tabelle, 65.536 Schemaobjekte und 65.536 unterschiedliche Spaltenlayouts je Tabelle. DDL-Vorschau maximal 2 MiB je erfasstem Statement.

Einzelne Datensätze bis 64 MiB können vollständig dekodiert werden. Größere Tupel bleiben indexierbar und als rohe SQL-Tupel exportierbar; normale Vorschau, Filter und CSV sind für diese Zeilen nicht verfügbar bzw. brechen dort ab. Die normale Zellvorschau ist höchstens 256 Byte vor Konvertierung, bei sehr breiten Tabellen kürzer; Details maximal 64 KiB je Feld und 2 MiB insgesamt. Kürzungen sind markiert. Für bytegenaue vollständige Werte das SQL-Rohtupel sichern.

## CSV-Konventionen

UTF-8 mit BOM, Semikolon, CRLF und doppelt gequotete Felder. Binärwerte bleiben Hex-/Bittext; echte NULL-Werte erscheinen als `\N`, ausgelassene Spalten als `[nicht im INSERT]`, NUL-Bytes in Text als `\0`. Gefährliche Formelpräfixe von Textfeldern und Spaltenüberschriften erhalten ein führendes Apostroph. Das reduziert Tabellenkalkulations-Formelausführung, ist aber keine allgemeine Sicherheitsgarantie für jeden Verbraucher.

Die Marker können mit tatsächlich vorkommendem Text kollidieren. Formelpräfix-Schutz und Zeichensatzersatz können den dargestellten Wert verändern. Deshalb ist CSV kein verlustfreies Backup- oder Roundtrip-Format. Bytegenaue SQL-Literale liefert der Rohtupel-Export; für ein vollständiges Backup die unveränderte Originaldatei aufbewahren.
