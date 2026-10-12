# Aus Text erstellen

Mit **Tools → Create from text** erzeugen Sie aus Rider-Text eine Ausgangsszene. Prüfen Sie das Ergebnis vor der Verwendung als Showdokumentation.

## Eingaben und Ablauf

Eingefügter Text sowie `.txt`- und `.pdf`-Dateien werden unterstützt; aus PDFs wird vor der Analyse Text extrahiert.

1. Öffnen Sie **Tools → Create from text** und fügen Sie Text ein oder laden Sie einen Rider.
2. Wählen Sie **Apply filter**, um mögliche Zeilen zu bereinigen; prüfen und bearbeiten Sie den gefilterten Text.
3. Wählen Sie **Create**.
4. Falls **Resolve fixture types** wegen ungeklärter Definitionen erscheint, prüfen Sie die Vorschläge und bestätigen Sie mit **Resolve and create** den Plan und ausgewählte Downloads.

## Scheinwerfertypen auflösen

Bekannte Typen verwenden das aktive Scheinwerferverzeichnis. Prüfen Sie bei unbekannten Typen zwischengespeicherte GDTF-Share-Vorschläge; verwenden Sie **Use suggested** für einen guten Treffer oder **Search...** zur Profilauswahl. Sie können einen Typ in der Tabelle korrigieren und **Create** abwählen, um die Scheinwerfer dieser Zeile auszulassen und andere Rider-Elemente zu behalten.

Die Prüfung benötigt keine Anmeldung. Der erste Katalogabruf oder ein Download kann sie erfordern. Bestätigte Zuordnungen werden erst nach Bestätigung für spätere Rider gespeichert. Ein Abbruch vor der Erstellung lässt Szene und Verzeichniszuordnungen unverändert.

### Generisches Ersatzprofil

**Use generic** liefert einen Ersatz für einen einzelnen Import ohne gespeicherte Dummy-Zuordnung. Fehlender Katalog, abgebrochene Anmeldung oder Netzwerkfehler erlauben weiterhin generische Erstellung. Behebbare Download-, Validierungs-, Modus-, Authentifizierungs- oder Verzeichnisfehler betreffen nur den jeweiligen Typ; der übrige Import läuft weiter.

### DMX-Modi

Wählen Sie bei mehreren Modi ausdrücklich den gewünschten Modus. Doppelklicken Sie auf die **Mode**-Zelle, um Optionen zu prüfen. Vorschläge ohne gewählten Modus verwenden generische Ersatzprofile. Bestätigte Änderungen am Verzeichnismodus werden nach **Resolve and create** gespeichert.

## Erstellte Inhalte und Parsermerkmale

Je nach Text entstehen Scheinwerfer, Traversen, Kettenzüge/Motoren und Szenenobjekte wie Bildschirme. Bildschirm- und Videosteuerungseinträge gelten nicht als Scheinwerfer.

Nützliche Muster sind Mengen (`12 Spot`), kombinierte Zeilen (`8 Wash + 4 Beam`), Positionen wie `LX1`, `FLOOR` und `SIDES` sowie Traversen-/Rohr- und Kettenzugeinträge. Rider-Import-[Einstellungen](preferences.md#import) steuern automatischen Patch, Ebenenorganisation und LX-Standardplatzierung.

## Autovervollständigung und Prüfung

Der Editor schlägt häufige Schlüsselwörter und lokale Verzeichnisbegriffe vor. Auf/Ab navigiert, Eingabe oder Tab übernimmt, Esc schließt die Vorschläge.

Prüfen Sie danach die Tabellen Fixtures, Trusses, Hoists und Objects, DMX-Modi und Patch. Kontrollieren Sie die Platzierung in [2D und 3D](views.md), speichern Sie PSTG und [exportieren Sie MVR](opening-mvr-files.md#mvr-exportieren), falls nötig.

Detaillierte Analyse- und Platzierungsregeln beschreibt [Text-to-scene rules](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/text_to_scene_rules.md).
