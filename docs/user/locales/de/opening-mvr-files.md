# MVR-Dateien öffnen

## Öffnen oder importieren

Wählen Sie **File → Import MVR...** und dann:

- **Open as new project**, um die Szene nach den üblichen Prüfungen auf ungespeicherte Änderungen zu ersetzen.
- **Merge into current project**, um Inhalte hinzuzufügen und Ihre bisherige Arbeit zu behalten.

Eine beim Start oder über eine unterstützte Dateizuordnung direkt geöffnete `.mvr`-Datei wird als neues Projekt geladen. Ein Abbruch der Importauswahl lässt das aktuelle Projekt unverändert.

Prüfen Sie die importierte Szene in [2D/3D](views.md) und in den Tabellen Fixtures, Trusses, Hoists und Objects. Mit dem [Inspektor](inspector.md) können Sie ein Paket ohne Import untersuchen.

## Konflikte bei der Zusammenführung

Bei UUID-Konflikten können Sie neue UUIDs für eingehende Objekte erzeugen (Standard; beide Szenen bleiben erhalten), passende Projektobjekte ersetzen oder kollidierende eingehende Objekte überspringen.

Verweist ein Scheinwerfertypname auf eine andere GDTF-Definition oder einen anderen Modus, verwenden Sie die aktuelle Projektdefinition, behalten Sie die importierte Definition unter einem neuen Typnamen oder brechen Sie ab. Eine abgebrochene oder fehlgeschlagene Zusammenführung stellt das aktuelle Projekt wieder her.

Doppelte DMX-Adressen erzeugen nicht blockierende Warnungen in der Console. Prüfen und korrigieren Sie den Patch nach der Zusammenführung.

## Ressourcen und Projektspeicherung

Importierte GDTFs, Symbole und Modelle bleiben in den Projektressourcen erhalten und können nach Speichern und erneutem Öffnen wieder aufgelöst werden. Prüfen Sie fehlende Profile und Modi vor dem Export; siehe [GDTF-Download](gdtf-download.md) und [Fehlerbehebung](troubleshooting.md).

Speichern Sie laufende Arbeiten als `.pstg`. Das erhält Perastage-Projektkontext, Layouts und Projektsymbole. MVR ist das Austauschformat. Perastage speichert eigene Austauschmetadaten in standardkonformen, anwendungseigenen UserData; andere Anwendungen behalten diese möglicherweise nicht.

Importierte Werte von FixtureID, FixtureIDNumeric und UnitNumber bleiben bei unterstützten Fixtures, Traversen, Supports und Szenenobjekten erhalten, einschließlich Nullwerten, Duplikaten und dem optionalen Vorhandensein von UnitNumber. Kopieren und Zusammenführen nummerieren diese Kennungen nicht automatisch neu. Werte außerhalb des unterstützten Ganzzahlbereichs blockieren Speichern und Export; bewahren Sie die Quelldatei auf. Die Nummerierung kann weiterhin ausdrücklich geändert werden.

## MVR exportieren

1. Wählen Sie **File → Export MVR...**.
2. Wählen Sie ein Ziel; wenn verfügbar, verwendet der vorgeschlagene Dateiname den Projektnamen.
3. Exportieren Sie und prüfen Sie das Ergebnis in der empfangenden Anwendung.

Die [Einstellungen](preferences.md#mvr-import---export) bieten eine Kompatibilitätsoption für Traversengeometrie. In PSTG gespeicherte Projektsymbole werden weder nach MVR exportiert noch in GDTFs eingebettet.
