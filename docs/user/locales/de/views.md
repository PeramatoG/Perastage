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

## Szenenobjekte ausrichten

Verwenden Sie die gemeinsame Symbolleiste **Ausrichtung** im 2D- oder 3D-Arbeitsbereich. Wählen Sie mindestens zwei effektive Ziele in Ansichten oder Tabellen aus. Wählen Sie **Gesamtausdehnung**, **Zuerst ausgewählt** oder **Zuletzt ausgewählt** und danach **Oben ausrichten**, **Unten ausrichten**, **Links ausrichten**, **Rechts ausrichten**, **Horizontal zentrieren** oder **Vertikal zentrieren**. Die erste/letzte Referenz folgt der Auswahlreihenfolge über Objekttypen hinweg und bleibt fest. Entfernen und erneutes Auswählen macht ein Objekt zum neuesten. Neue Sammelauswahlen verwenden eine stabile UUID-Reihenfolge; Tabellensortierung erhält die bisherige Reihenfolge.

Referenz und Ebene sind zwei exklusive Symbolgruppen mit jeweils einer aktiven Option. Bewegen Sie den Zeiger über ein Symbol, um die Hilfe anzuzeigen. In 2D folgt die Ebene automatisch der Ansicht: **XY** für Oben, **XZ** für Vorne und **YZ** für Seite. In 3D verwenden die Schaltflächen **XY/XZ/YZ** dieselbe Ebene zum Ausrichten und Ziehen der Auswahl. Standardansichten aktualisieren die Ebene; perspektivisches Orbitieren behält sie bei. Anfangs gilt XY. Beim Ausrichten bleiben dritte Koordinate, Drehung und Skalierung erhalten; die Seitenansicht spiegelt Y. Die Achsensperre wählt eine Richtung innerhalb der Ebene; lokale Achsen werden auf sie projiziert. Freies Ziehen nutzt die Ebene durch die Auswahl. Bei einer Ansicht auf die Kante wechseln oder drehen Sie die Ansicht. Magnet ist eine Ausnahme: Das Einrasten darf die Ebene verlassen, um ein nahes Element zu erreichen. Kontinuierliches Platzieren und Verteilen behalten ihr Verhalten. Gruppen folgen den Bewegungseinstellungen; ausgewählte Nachkommen werden nie doppelt verschoben.

Eine geänderte Ausrichtung erzeugt einen Rückgängig-Schritt; Wiederholung ohne Änderung erzeugt keinen Verlauf. Fehlende physische Geometrie bricht den gesamten Vorgang ab; die Diagnose nennt UUID/Ressource. Dummy-Kettenzüge ohne physische Geometrie können nicht anhand ihrer Ausdehnung ausgerichtet werden. Layout-Ausrichtung ist nicht enthalten.
