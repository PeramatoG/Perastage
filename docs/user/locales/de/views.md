# Ansichten

Prüfen Sie mit 3D räumliche Beziehungen und Ausrichtungen, mit 2D Pläne, Positionen und Lesbarkeit. Ausgeblendete Ansichtsbereiche öffnen Sie über **View**.

## Navigieren und prüfen

Ziehen Sie in 3D zum Orbitieren, mit Umschalt oder mittlerer Maustaste zum Verschieben und nutzen Sie das Mausrad zum Zoomen. Rechtsklick und **Render style** ändern die Darstellung, einschließlich Farben nach Typ, Ebene oder Universum.

Ziehen Sie in 2D auf freier Fläche zum Verschieben und nutzen Sie das Mausrad zum Zoomen. Wählen Sie Szenenelemente, um ihre Zeilen in Fixtures, Trusses, Hoists und Objects abzugleichen. Doppelklicken Sie eine Zelle innerhalb einer Mehrfachauswahl, um diese Spalte für die ausgewählten Zeilen zu bearbeiten.

## Positionswerkzeug

Öffnen Sie **View → Position Tool** im 2D- oder 3D-Arbeitsbereich. Dasselbe Fenster lässt sich andocken, lösen, skalieren oder ausblenden; Position und Sichtbarkeit werden mit dem Arbeitsbereich gespeichert. Ohne Auswahl sind die Koordinatenfelder deaktiviert.

X, Y und Z teilen sich eine flexible Zeile in einem kompakten Fenster.

Das Fenster erscheint in 2D und 3D standardmäßig unter Summary in der rechten Spalte. Die Pfeile jeder Achse verschieben jedes effektive Ziel pro Klick um 1 mm in metrischen Einheiten oder 0,01 Fuß in imperialen Einheiten. Unterschiedliche Werte und gespeicherte Präzision bleiben erhalten. Jeder Klick lässt sich rückgängig machen.

X, Y und Z sind absolute Weltkoordinaten des Einfügepunkts. Bearbeiten Sie ein Feld und drücken Sie die Eingabetaste oder verlassen Sie das Feld, um den Wert anzuwenden. Die anderen Achsen, Drehung und Skalierung bleiben erhalten. Escape verwirft die noch nicht bestätigte Eingabe. Jede bestätigte Änderung bildet einen Rückgängig-Schritt. Leere oder ungültige Eingaben verschieben nichts.

Bei Mehrfachauswahl werden gemeinsame Koordinaten angezeigt; unterschiedliche Koordinaten bleiben leer. Ein eingegebener Wert setzt diese absolute Koordinate für jedes effektive Ziel einzeln. Gruppierte Kinder folgen den Einstellungen unter **Selection & Movement**.

Kleine rote Quadrate markieren die effektiven Einfügepunkte in den Ansichten Oben, Unten, Vorne und Seite des interaktiven 2D-Viewers. Ihre Bildschirmgröße bleibt konstant. Sie erscheinen nicht in Aufnahmen, gedruckten Layouts oder Exporten. In 3D werden keine Markierungen angezeigt.

Die Einheiten folgen **Preferences → Units**: Meter mit drei Nachkommastellen oder das vorhandene imperiale Fußformat. Einheitensuffixe und Eingaben in Fuß/Zoll werden unterstützt. Die Anzeige rundet keine gespeicherten Positionen.

## Verschieben und auswählen

- **Drag Move** aktiviert das Ziehen von Szenenauswahlen mit der linken Maustaste; standardmäßig deaktiviert.
- **Axis Lock** beschränkt Bewegungen. Ohne Sperre bewegen Sie frei in 2D oder auf einer zur Kamera ausgerichteten Ebene in 3D.
- **Local Axes** ändert die Transformationsachsen der Ansicht.
- **Magnet** rastet beim Verschieben oder Einfügen an kompatiblen Ankern ein.
- **Cross-table Actions** erlaubt Ansichtsaktionen für Objekttypen außerhalb der aktiven Tabelle; standardmäßig deaktiviert.

Gruppenbewegungen folgen den [Einstellungen für Auswahl und Bewegung](preferences.md#auswahl-und-bewegung). Prüfen Sie die Endposition in beiden Ansichten und verwenden Sie bei Bedarf Undo.

Tastatur-, Maus- und Console-Eingaben beschreibt [Tastenkürzel und Befehlsleiste](shortcuts-and-command-bar.md). Druckseiten erstellen Sie mit [Layouts und PDF](layouts.md).
