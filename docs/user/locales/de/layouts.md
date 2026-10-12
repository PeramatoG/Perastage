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
