# Architektur und Speicherverhalten

## Originaldatei statt SQL-Reimport

Ein 1-MiB-Lesepuffer scannt den SQL-Bytestrom. Kommentare, Literale, Escapes, Klammern und DELIMITER-Anweisungen bestimmen Statement- und Tupelgrenzen. Bei einem großen erweiterten `INSERT ... VALUES (...),(...),...` wird nicht das vollständige Statement gehalten. Pro Tupel wird ein Positionsdatensatz geschrieben. Der Dump selbst wird weder ausgeführt noch in SQLite/MySQL umgeschrieben.

Die Begriffe Tabelle und Datenbank beschreiben hier den Namensraum des Dumps, nicht eine aktive Engine. Die sichtbaren Einträge sind die gefundenen VALUES-Tupel. Der Endzustand nach Ausführung eines beliebigen SQL-Skripts wird nicht simuliert.

## Festplattenformat

Jede Tabelle hat eine `.bli`-Datei mit 16 Bytes pro Tupel: 8 Byte Originalposition, 4 Byte Tupellänge und 4 Byte Spaltenlayoutkennung. Das oberste Bit der Layoutkennung markiert `NO_BACKSLASH_ESCAPES`. Die feste Indexbreite ermöglicht direkten Zugriff auf beliebige Seiten ohne `OFFSET`-Scan durch Millionen Datensätze. Das Cache-Manifest speichert Schema, Tabellen, Spaltenlisten, Statistiken und Importhinweise; Formatkennung `BLDUMPIDX04`.

1.000.000 Tupel = 16.000.000 Byte = ungefähr 15,26 MiB Positionsindex, zuzüglich Manifest/Schema und Dateisystemaufwand. Ein Volltrefferfilter kann weitere 16 Bytes je Tupel benötigen. CSV benötigt zusätzlichen Platz in Größe der tatsächlichen Ausgabe. Bei Neuindexierung können alte Caches anderer Dateiversionen weiter im Profil liegen.

Manifest und Indexgrößen werden beim Laden geprüft. Die Cachekennung berücksichtigt Dateipfad, Größe, Änderungszeit und Hash-Stichproben aus den ersten und letzten 64 KiB. **Keine kryptografische Vollprüfung:** Änderungen im Mittelteil mit identischer Größe und zurückgesetzter Zeit können unerkannt bleiben. Daher Dumps nicht unter der Anwendung austauschen; bei Zweifel `Neu indexieren` verwenden. Unter Windows hält die Anwendung die geöffnete Quelldatei ohne Schreib-/Löschfreigabe offen.

## Arbeitsspeicher

Der Index wächst auf der Platte, nicht als Zeilenliste im RAM. Zusätzlich zum Streaming-Lesepuffer gibt es blockweise Index-I/O, Schema-/Spaltenlayoutdaten und die aktuelle GUI-Seite. Beim Blättern werden höchstens 100 Tupel dekodiert und ihre begrenzten Zellvorschauen gespeichert. Ein 1-MiB-Quellpuffer beschleunigt aufeinanderfolgende Zugriffe. Beim Filter oder Export wird ein Tupel nach dem anderen dekodiert und anschließend freigegeben.

Der Speicherbedarf ist **nicht pauschal konstant oder garantiert auf wenige MiB begrenzt**: Sehr breite Tabellen, viele unterschiedliche Spaltenlayouts und große einzelne Werte benötigen zusätzlichen Speicher. Pro Datensatz können Rohbytes, dekodierte Werte, temporäre Zeichensatzkonvertierung und Vorschau gleichzeitig entstehen. Ein einzelnes 64-MiB-Tupel kann daher deutlich mehr als 64 MiB RAM benötigen. Hinzu kommen Windows-Fenster, Schrift-/Iconressourcen und Systembibliotheken. Die Linux-Kernmessung ist keine Messung der Windows-GUI.

## Threads und Filter

Ein Hintergrundjob besitzt die Datei-/Kernoperation. Der GUI-Thread bedient ausschließlich Steuerelemente und die veröffentlichte Seite. Es läuft maximal ein Datei-Job pro Fenster; währenddessen werden widersprüchliche Bedienelemente deaktiviert. Fortschritt und Abbruch werden atomar übergeben, fertige Ergebnisse per Fensternachricht. Ein exklusives `session.lock` verhindert die gleichzeitige Nutzung desselben Cacheverzeichnisses durch mehrere Instanzen.

Eine Filterabfrage scannt die aktuelle Tabelle vollständig. Enthält-Suche verwendet KMP für lineare Laufzeit im Werttext. Trefferpositionen werden auf Platte geschrieben; Seiten aus dem fertigen Trefferindex bleiben direkt adressierbar. Es gibt keinen Volltextindex, keine MySQL-Collation und keine numerische Typauswertung. Ein neuer Filter ersetzt den alten und bezieht sich wieder auf die gesamte Tabelle.

## Ausgaben und Abbruch

CSV und SQL-Rohtupel werden zunächst als `ziel.bitline-part-PROZESSID` geschrieben. Erst nach erfolgreichem Schreiben/Schließen ersetzt ein Dateiverschieben das Ziel. Beim kontrollierten Abbruch wird die Teildatei entfernt und ein vorhandenes Ziel bleibt bestehen. Nach Stromausfall oder Prozessabbruch können temporäre Dateien übrig bleiben. Vor Nutzung mit wichtigen Daten sollte die offene Windows-Abnahme durchgeführt werden.

Ein SQL-Rohtupel ist ausschließlich der bytegenaue Bereich `(Wert1,Wert2,...)`; kein vollständiges INSERT und kein selbständiges Backup. CSV ist ein Text-/Analyseformat mit den im Handbuch beschriebenen Markierungen und Schutzänderungen.
