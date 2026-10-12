# MVR / GDTF-Inspektor

Öffnen Sie **View → Layout Views → MVR / GDTF Inspector**, um einen Stand des aktuellen Projekts zu untersuchen. Die Prüfung ist schreibgeschützt: Sie ändert weder Projekt noch geprüfte Datei oder Projektauswahl und markiert das Projekt nicht als bearbeitet.

## Prüfquelle auswählen

- Die Projektansicht ist eine Momentaufnahme, keine laufend aktualisierte Szene. Nutzen Sie **Refresh** nach Änderungen, um den neuesten Stand zu erfassen.
- Mit **Open external...** prüfen Sie externe `.mvr`- oder `.gdtf`-Dateien ohne Import.

## MVR- und GDTF-Inhalte durchsuchen

Bei MVR navigieren Sie im Paketbaum zu Archiveinträgen oder im Szenenbaum zu Objekten mit Namen, Typen und UUIDs. Aktivieren Sie ein eingebettetes GDTF, um seine Scheinwerferdefinition zu öffnen, und kehren Sie danach zum übergeordneten MVR zurück.

Bei GDTF prüfen Sie FixtureType-Identität, physikalische Informationen und Revisionen. Wählen Sie einen DMX-Modus, um Kanäle, Funktionen und Sets zu untersuchen. Die Detailseiten zeigen Räder, Positionen, Filter und Ressourcenverweise.

## Quelle und Vorschau

Wählen Sie eine Paketressource für Quelle oder Vorschau. XML und Klartext erscheinen in **Source**; SVG bietet XML und eine gerenderte Vorschau. Unterstützte Bilder sowie GLB/3DS-Modelle erscheinen in **Preview**. Nicht unterstützte oder zu große Ressourcen bleiben mit Vorschau-Status aufgelistet.

Nutzen Sie Suche, Einklappen und Kopieren für XML. Große Dokumente können mit einer begrenzten Ansicht beginnen und eine ausdrückliche Aktion zum Laden des vollständigen Texts anbieten. Ziehen Sie Bereichstrenner, um Navigation, Quelle, Details oder Vorschau mehr Platz zu geben.

## Probleme und Diagnosen

**Issues** gruppiert gemeldete Probleme; **Diagnostics** liefert weitere Untersuchungsdetails. Eine Vorschaugrenze allein bedeutet keine ungültige Datei. Der Inspektor repariert keine Pakete und lädt keine fehlenden Ressourcen nach. Korrigieren Sie mit den normalen Bearbeitungsabläufen oder [exportieren Sie einen Diagnosebericht](troubleshooting.md#protokolle-und-diagnoseberichte), wenn Sie ein Problem melden.

## Scheinwerferleistung für die Planung

Die Scheinwerferleistung ist die maximale elektrische Leistungsaufnahme für die Planung. Ein fehlender Wert erscheint im Editor und in der Tabelle als **—**; eine ausdrücklich deklarierte Null bleibt Null. Beam-Leistung dient nur zur Information und ersetzt keine fehlende Eingangsleistung. Die GDTF-Übersicht zeigt die elektrische Quelle und die informative Beam-Summe. Prüfen Sie Issues/Diagnostics auf mehrdeutige Eingänge, widersprüchliche Angaben oder Beam-Summen über der deklarierten Eingangsleistung.

Leistungsänderungen im normalen Editor oder in der Tabelle erzeugen eine Projektkorrektur für diesen Scheinwerfer. Berechnungen verwenden sie sofort; PSTG speichert sie ohne Änderung des Quellarchivs. Andere Instanzen behalten eigene Werte. Ausdrückliche GDTF-Dokumentkorrekturen erfordern ein vorhandenes sicheres elektrisches Feld; Perastage bewahrt Anschlussinformationen und erfasst eine standardmäßige Revision. Fehlt ein sicheres Feld, behalten Sie die Korrektur im Projekt, statt Anschlüsse oder Versorgungstopologien zu erfinden. Projekt-Leistungsüberschreibungen werden beim MVR-Export nicht automatisch in GDTFs geschrieben.

## Vorschauen im Scheinwerfereditor

Im Fixture Editor beginnt Preview mit **All fixture**. Wählen Sie eine Geometry für die Prüfung ihres Teilbaums im Solo-Modus; wiederholte Model-Instanzen bleiben getrennte Auswahlmöglichkeiten. Das GDTF-Vorschaubild darunter unterstützt kanonische PNG und SVG im Wurzelverzeichnis.

Wählen Sie unter Symbols ein **Standard GDTF** Model für seine exakten Top/Front/Side-Ressourcen, Abmessungen und PrimitiveType. Tooltips zeigen Pfade, Einfügeversatz und Diagnosen. Eine vorhandene Ressource kann eine nicht unterstützte Vorschau haben. Die unabhängige Gruppe **Perastage project (PSTG)** zeigt scheinwerferweite Top/Front/Side/Bottom-Ansichten mit eigener Herkunft für Benutzerüberschreibungen. Das Öffnen der Vorschauen und Erzeugen von Projektsymbolen erhält die ursprünglichen GDTF-Ressourcen.
