# Layouts und PDF

Layouts ordnen Szenenansichten und ergänzende Informationen auf druckbaren Seiten an.

## Seite vorbereiten

1. Öffnen Sie **View → Layout Views → Layout Mode View**. Blenden Sie bei Bedarf **Layouts** über **View** ein.
2. Wählen Sie ein Layout oder erstellen Sie eines mit **Add**. **Rename** und **Delete** verwalten Seiten; das letzte Layout kann nicht gelöscht werden.
3. Wählen Sie die Ausrichtung im Kontextmenü des Layouts.
4. Fügen Sie Elemente über die Layout-Werkzeugleiste hinzu:

| Aktion | Inhalt |
| --- | --- |
| **Add 2D View** | 2D-Szenenansicht |
| **Add Legend** | Scheinwerferlegende |
| **Add Event Table** | Veranstaltungsinformationen |
| **Add Text** | Hinweise und Überschriften |
| **Add Image** | Bilder, etwa Logos |

Bearbeiten Sie die verfügbaren Einstellungen jedes Elements für Inhalt und Darstellung. Prüfen Sie Szenendaten in [Ansichten und Tabellen](views.md), bevor Sie Dokumentation erstellen.

## Anordnen und bearbeiten

Klicken Sie auf einen Rahmen, um ihn auszuwählen. **Strg + Klick** schaltet die
Auswahl um, **Umschalt + Klick** fügt ein Element hinzu. **Umschalt + Ziehen** auf
leerem Seitenbereich wählt sich schneidende Rahmen von hinten nach vorn aus.
Bei gleicher Ebene gilt die Reihenfolge 2D-Ansicht, Legende, Ereignistabelle,
Text, Bild und danach die gespeicherte Reihenfolge. Ein Klick trifft den vordersten
Rahmen. Das zuletzt gewählte verbleibende Element ist primär (dunklerer Umriss).
Entfernen und erneutes Auswählen setzt es ans Ende. Die Dokumentauswahl ist von
der Szene unabhängig und wird beim Seitenwechsel gelöscht.

Ziehen Sie einen gewählten Rahmen, um die gesamte Auswahl ohne Größenänderung
oder Änderung der relativen Positionen zu bewegen. Größenanfasser erscheinen
nur bei Einzelauswahl. Doppelklicken Sie einen Rahmen, um nur dieses Element zu
bearbeiten. **Entf** löscht die gesamte Auswahl in einem Rückgängig-Schritt.
Das Kontextmenü einer Mehrfachauswahl bietet gemeinsames Löschen an. Ein Klick
ins Leere hebt die Auswahl auf; Ziehen ohne Zusatztaste verschiebt die Seite.
Das Mausrad zoomt, **Z** passt die Seite ein.

Die gemeinsame Symbolleiste **Ausrichtung** unterstützt gemischte Auswahlen mit
mindestens zwei Elementen. Wählen Sie Gesamtausdehnung, Zuerst ausgewählt oder
Zuletzt ausgewählt und eine der sechs Kanten- oder Zentrierungsaktionen. Die
Referenz folgt der Auswahlreihenfolge und bleibt bei Zuerst/Zuletzt unverändert.
Geometrische Grenzen verwenden vollständige Seitenrahmen; Einfügepunkte sind
deren linke obere Seitenecken, niemals die Einfügepunkte der Geräte innerhalb
einer Ansicht. Arbeitsebenen sind im Layout-Modus deaktiviert.

Ansichten behalten Bruchteile von Punkten. Andere Elemente runden Zielpositionen
auf ganze Punkte, halbe Punkte zur positiven Unendlichkeit; der Fehler beträgt höchstens 0,5 pt
je Achse. Gemischtes Ziehen rundet den gemeinsamen Versatz auf ganze Punkte und
erhält relative Positionen. Bei Auswahlen mit Ganzzahlrahmen gilt eine
Gesamtausdehnungs-Ausrichtung innerhalb von 0,5 pt als abgeschlossen, damit
Wiederholungen keine Drift verursachen. Maßstäbe, Kameras, Filter und
Szenenpositionen bleiben erhalten. Auswahlumrisse werden nicht gedruckt.

Speichern Sie das `.pstg`-Projekt, um Layouts zu behalten. **Export template** erstellt ein portables `.pslayout`-Paket mit referenzierten Bildern. **Import template** akzeptiert `.pslayout` und ältere `.json`-Vorlagen.

## PDF exportieren

Wählen Sie ein nicht leeres Layout und **File → Print Layout...**. Wählen Sie ein beschreibbares `.pdf`-Ziel. Das PDF verwendet Seitengröße und Ausrichtung des gewählten Layouts und enthält unterstützte Elemente. Prüfen Sie es vor Druck oder Weitergabe.

Bei fehlenden Ansichten, falscher Ausgabe oder Exportfehlern siehe [Fehlerbehebung](troubleshooting.md#layout--und-pdf-probleme).

## Dokumenttypografie

Wählen Sie unter **Schriftfamilie** PerastageSans, Noto Serif, Source Sans 3 oder Noto Sans Mono für Gerätebeschriftungen, Legenden, Ereignistabellen und Texte. Markieren Sie im Texteditor eine Passage, um deren Schrift zu ändern. Ohne Markierung gilt die Auswahl für das gesamte Element. Die PDF-Datei bettet zugelassene Schriften ein und macht unterstützten Text auswählbar. Die **Dokumentvorschau** zeigt die Darstellung im Layout. **Schriften aktualisieren** prüft die zugelassenen Schriftdateien erneut.

Fehlt eine Schrift, behält Perastage die gespeicherte Auswahl bei, verwendet eine zugelassene Ersatzschrift und meldet die Ersetzung in den Exportdetails. Öffnen Sie das Projekt erneut, sobald die Schrift wieder verfügbar ist.

## Physischer Maßstab und Ansichtseigenschaften

Doppelklicken Sie auf eine 2D-Ansicht und öffnen Sie **Ansichtseigenschaften**. **Frei / Variabel** bleibt der Standard für neue Ansichten und alte Dateien. Zoom, Verschieben und visuelles Einpassen bleiben verfügbar. Der ungefähre effektive Maßstab dient nur zur Information. **Visueller Ausschnitt** verändert den visuellen Ausschnitt, keinen physischen Maßstab.

Wählen Sie **Fest** für 1:25, 1:50, 1:100, 1:200, 1:500 oder einen eigenen Nenner zwischen 0,01 und 10000000. Ein Meter belegt bei 1:50 genau 20 mm, bei 1:100 genau 10 mm auf Papier. Der Mittelpunkt verwendet Meter und die Achsen oben X/Y, vorne X/Z, seitlich Y/Z; die seitliche horizontale Anzeige läuft in Richtung −Y. Geben Sie den Mittelpunkt ein oder verschieben Sie die Ansicht direkt im Editor. Im festen Modus zeigt der rote Rahmen den tatsächlichen Ausschnitt im gewählten Maßstab. Zoom, Einpassen und Ausschnittregler sind gesperrt; Ziehen und Pfeiltasten ändern nur den Mittelpunkt. Der freie Modus stellt den vorherigen Zoom wieder her und behält den neuesten Mittelpunkt.

Beim Moduswechsel bleibt der aktuelle Mittelpunkt in beiden Richtungen erhalten. Verschieben oder numerische Mittelpunktänderungen im festen Modus bleiben beim Entsperren erhalten; der vorherige freie Zoom wird wiederhergestellt. Beide Modi verwenden für die Layout-Vorschau denselben üblichen 2D-Szenenrenderer und dieselben Symbole.

Millimeter werden ohne Dezimalstellen angezeigt. Eingegebene Bruchteile und die interne Genauigkeit unveränderter Felder bleiben erhalten.

Die Pfeile ändern die Rahmengröße um jeweils 1 mm, den Mittelpunkt um 0,01 m und den Maßstabsnenner um 1; interne Bruchteile bleiben erhalten. Manuelle Eingabe und gängige Maßstäbe bleiben verfügbar. Im festen Modus zeigt ein sichtbarer Hinweis den Maßstab und erklärt die Zoomsperre.

Gültige Änderungen in **Ansichtseigenschaften** werden sofort im Editor angezeigt, einschließlich Modus, Maßstab, Mittelpunkt und Rahmengröße. Unvollständige oder ungültige Zahlen behalten die letzte gültige Vorschau bei. **Cancel** oder das Schließen des Dialogs stellt die vorherigen Eigenschaften und den Ausschnitt wieder her; **OK** übernimmt die Änderungen in den Editorentwurf.

Rahmenbreite und -höhe verwenden Millimeter. Verschieben Sie das Element auf der Layout-Seite, um seine Position zu ändern; X/Y werden in diesem Dialog nicht angeboten. Bruchteile bleiben erhalten. Teilweise außerhalb der Seite liegende Rahmen sind erlaubt. Die Mindestgröße beträgt 1 Punkt (0,353 mm), die Grenze 200000 Punkte. Verschieben und Größenänderungen fester Rahmen erhalten Mittelpunkt und Maßstab und ändern den sichtbaren Bereich. Layout-Zoom und DPI betreffen nur die Anzeige. Der Wechsel zu frei erhält den vorherigen Zoom, den neuesten Mittelpunkt, Filter und Darstellung. **Cancel** verwirft den Entwurf; **OK** bestätigt mit Rückgängig/Wiederholen und PSTG/PSLAYOUT-Speicherung.

Im Seitenkontextmenü wählen Sie A3/A4 und die Ausrichtung, ohne Inhalte zu skalieren. Feste Ansichten benötigen PDF-Ausgabe zu 100 % im Layout-Papierformat; andere Formate werden mit Erklärung abgelehnt. Ändern Sie zuerst das Layout-Papier, wenn ein anderes Format benötigt wird. Freie Seiten behalten die gleichmäßige Ausgabeanpassung. Drucken Sie PDF in tatsächlicher Größe/100 %; Perastage kontrolliert keine Treiberskalierung. Der physische Vertrag gilt für Geometrie und erweitert keine bestehenden Typografiegarantien.

## Wiederverwendbare Stile für Gerätebeschriftungen

Klicken Sie mit der rechten Maustaste auf einen 2D-Ansichtsrahmen und wählen Sie
**Einstellungen für Gerätebeschriftungen...**. Übernehmen Sie die Projektdarstellung
oder wählen Sie einen benannten Stil. Sie können aus aktuellen Einstellungen einen
Stil erstellen, ihn bearbeiten, umbenennen oder duplizieren. Änderungen gelten für
alle Ansichtsfenster mit diesem Stil, auch auf anderen Seiten. Verwendete Stile
lassen sich erst löschen, nachdem die zugehörigen Ansichtsfenster wieder erben.

**Aktuelles Ansichtsfenster anpassen...** öffnet den vorhandenen Editor für das
aktuelle Ansichtsfenster oder dessen ausgewählte Geräte. Wählen Sie die Geräte
vorher in der Szene aus. Vorschau, Hintergrundwahl und Ziehen bleiben verfügbar.
Jedes bearbeitete Feld einer Orientierung speichert seine vollständigen Eigenschaften;
unbearbeitete Felder erben. Vorrang: lokales Gerät, Ansichtsfensterfeld, Stilfeld,
Projektgerät, Projektvorgabe, bisherige Konfiguration. Lokales Zurücksetzen erhält
Projektanpassungen. Das Wiederherstellen geerbter Einstellungen im Ansichtsfensterdialog
entfernt auch die Stilzuweisung.

Im eingebetteten Editor bleibt Übernehmen ein Entwurf; OK übernimmt ihn in den
äußeren Dialog. Erst dessen OK erzeugt einen Rückgängig-Schritt, Abbrechen verwirft
die Änderungen. Die Wahl eines Stils oder Hintergrunds verändert das Projekt nicht.
PSTG speichert Stile und lokale Einstellungen. PSLAYOUT enthält benötigte Stile und
ordnet kollidierende Kennungen beim Import deterministisch neu zu. Fehlende Referenzen
blockieren den Vorgang mit Diagnose. MVR und GDTF bleiben unverändert.
Größen und Versätze bleiben in Szenenmetern und haben bei unterschiedlichen Maßstäben
keine konstante Papiergröße. Bottom behält aus Kompatibilitätsgründen den Side-Stilschlüssel
und die Top-Projektion; es entsteht keine neue standardisierte GDTF-SVG-Orientierung.

Klicken Sie im **2D View Editor** auf ein Gerät, um dessen Beschriftungen auszuwählen; Umschalt-Klick fügt Geräte hinzu oder entfernt sie. **Edit Fixture Labels...** öffnet den ausgewählten lokalen Bereich. Ziehen verschiebt weiterhin die Kamera, niemals die Geräte. Projektauswahl und Verlauf bleiben unverändert. Kontur und Füllung der Bildschirmbeschriftungen verwenden dieselbe Rasterung und Pixelrundung mit der bisherigen Konturstärke der Szene, auch bei hoher Pixeldichte. PDF behält seine Maße in Szenenmetern.

Wählen Sie unter **Schriftfamilie** eine mitgelieferte oder eine installierte Schrift für Gerätebeschriftungen, Legenden, Ereignistabellen und Text. **Schriften aktualisieren** übernimmt Änderungen an installierten Schriften. Das Projekt speichert die Auswahl und verwendet auf einem anderen Computer bei Bedarf eine Ersatzschrift. Der PDF-Export bettet kompatible TrueType-Schriften ein, wenn ihre Berechtigungen dies zulassen, und meldet Ersetzungen.
