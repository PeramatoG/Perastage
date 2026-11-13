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

Ziehen Sie einen Rahmen zum Verschieben und einen Griff zur Größenänderung. Doppelklick bearbeitet das Element. Rechtsklick bietet Bearbeitung, Löschen und Stapelreihenfolge. Ziehen auf freier Fläche verschiebt die Ansicht, das Mausrad zoomt. **Z** passt das Layout ein; **Delete** entfernt das ausgewählte Element.

Speichern Sie das `.pstg`-Projekt, um Layouts zu behalten. **Export template** erstellt ein portables `.pslayout`-Paket mit referenzierten Bildern. **Import template** akzeptiert `.pslayout` und ältere `.json`-Vorlagen.

## PDF exportieren

Wählen Sie ein nicht leeres Layout und **File → Print Layout...**. Wählen Sie ein beschreibbares `.pdf`-Ziel. Das PDF verwendet Seitengröße und Ausrichtung des gewählten Layouts und enthält unterstützte Elemente. Prüfen Sie es vor Druck oder Weitergabe.

Bei fehlenden Ansichten, falscher Ausgabe oder Exportfehlern siehe [Fehlerbehebung](troubleshooting.md#layout--und-pdf-probleme).

## Physischer Maßstab und Ansichtseigenschaften

Doppelklicken Sie auf eine 2D-Ansicht und öffnen Sie **Ansichtseigenschaften**. **Frei / Variabel** bleibt der Standard für neue Ansichten und alte Dateien. Zoom, Verschieben und visuelles Einpassen bleiben verfügbar. Der ungefähre effektive Maßstab dient nur zur Information. **Visueller Ausschnitt** verändert den visuellen Ausschnitt, keinen physischen Maßstab.

Wählen Sie **Fest** für 1:25, 1:50, 1:100, 1:200, 1:500 oder einen eigenen Nenner zwischen 0,01 und 10000000. Ein Meter belegt bei 1:50 genau 20 mm, bei 1:100 genau 10 mm auf Papier. Der Mittelpunkt verwendet Meter und die Achsen oben X/Y, vorne X/Z, seitlich Y/Z; die seitliche horizontale Anzeige läuft in Richtung −Y. Geben Sie den Mittelpunkt ein oder verschieben Sie die Ansicht direkt im Editor. Im festen Modus zeigt der rote Rahmen den tatsächlichen Ausschnitt im gewählten Maßstab. Zoom, Einpassen und Ausschnittregler sind gesperrt; Ziehen und Pfeiltasten ändern nur den Mittelpunkt. Der freie Modus stellt den vorherigen Ausschnitt wieder her.

Millimeter werden ohne Dezimalstellen angezeigt. Eingegebene Bruchteile und die interne Genauigkeit unveränderter Felder bleiben erhalten.

Gültige Änderungen in **Ansichtseigenschaften** werden sofort im Editor angezeigt, einschließlich Modus, Maßstab, Mittelpunkt und Rahmengröße. Unvollständige oder ungültige Zahlen behalten die letzte gültige Vorschau bei. **Cancel** oder das Schließen des Dialogs stellt die vorherigen Eigenschaften und den Ausschnitt wieder her; **OK** übernimmt die Änderungen in den Editorentwurf.

Seitenposition X/Y und Rahmenbreite/-höhe verwenden Millimeter mit Ursprung oben links. Bruchteile bleiben erhalten. Teilweise außerhalb der Seite liegende Rahmen sind erlaubt. Die Mindestgröße beträgt 1 Punkt (0,353 mm), die Grenze 200000 Punkte. Verschieben und Größenänderungen fester Rahmen erhalten Mittelpunkt und Maßstab und ändern den sichtbaren Bereich. Layout-Zoom und DPI betreffen nur die Anzeige. Der Wechsel zu frei erhält die vorherige Kamera, Filter und Darstellung. **Cancel** verwirft den Entwurf; **OK** bestätigt mit Rückgängig/Wiederholen und PSTG/PSLAYOUT-Speicherung.

Im Seitenkontextmenü wählen Sie A3/A4 und die Ausrichtung, ohne Inhalte zu skalieren. Feste Ansichten benötigen PDF-Ausgabe zu 100 % im Layout-Papierformat; andere Formate werden mit Erklärung abgelehnt. Ändern Sie zuerst das Layout-Papier, wenn ein anderes Format benötigt wird. Freie Seiten behalten die gleichmäßige Ausgabeanpassung. Drucken Sie PDF in tatsächlicher Größe/100 %; Perastage kontrolliert keine Treiberskalierung. Der physische Vertrag gilt für Geometrie und erweitert keine bestehenden Typografiegarantien.
