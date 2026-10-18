# GDTF-Download

Mit **Tools → Download GDTF** suchen und laden Sie Scheinwerferprofile von GDTF Share. Online-Katalog und Downloads benötigen ein GDTF-Share-Konto und eine Internetverbindung.

## Profil finden

1. Öffnen Sie **Tools → Download GDTF**. Ein nutzbarer Cache-Katalog öffnet sofort; andernfalls melden Sie sich zum Laden des Katalogs an. Der Abbruch der ersten Anmeldung beendet den Vorgang.
2. Suchen Sie nach Hersteller, Scheinwerfer, UUID, Revision oder DMX-Modusname. Alle Suchwörter müssen in beliebiger Reihenfolge passen; Manufacturer- und Fixture-Filter begrenzen die Ergebnisse.
3. Wählen Sie eine Revision und prüfen Sie vor dem Download Ersteller, Datum, Größe, Kennungen und DMX-Modi mit ihrer Kanalbelegung.
4. Laden Sie das ausgewählte Profil herunter und prüfen Sie Definition und Modus im Projekt oder Verzeichnis.

## Zwischengespeicherte und Offline-Suche

Ein aktueller Katalog wird eine Stunde lang wiederverwendet. Veraltete Cache-Daten werden im Hintergrund aktualisiert, während die Suche verfügbar bleibt. Bei fehlgeschlagener Aktualisierung bleiben Cache-Ergebnisse nutzbar. Die Cache-Suche funktioniert offline ohne Anmeldung. Downloads benötigen Online-Authentifizierung und warten auf den Abschluss einer laufenden Aktualisierung.

Ein fehlgeschlagener oder abgebrochener Download erhält ein vorhandenes Zielprofil.

## Zugangsdaten und lokale Profile

Passwörter verwenden den sicheren Zugangsdaten-Speicher des Betriebssystems, sofern verfügbar. Andernfalls warnt Perastage und kann die Daten für den aktuellen Vorgang nutzen, ohne das Passwort zu speichern. In einer späteren Sitzung müssen Sie es möglicherweise erneut eingeben. Der Benutzername kann als Anmeldehilfe gespeichert werden.

Mit **Tools → Edit dictionaries** verwalten Sie Profilzuordnungen; **Tools → Open user library folder** öffnet lokale Inhalte. Falls Passwörter nicht erhalten bleiben, siehe [Fehlerbehebung für Zugangsdaten](troubleshooting.md#gdtf-share-zugangsdaten).

## Profile beim MVR-Import auflösen

Der Dialog MVR/App/Download bleibt erhalten. Download öffnet die Warteschlange
mit sicheren Vorschlägen und verfügbaren MVR- oder Anwendungsprofilen als Ersatz.
Wählen Sie eine Zeile, um Revisionen und Warnungen zu prüfen, auch nach einem
Erfolg. **Auswahl anwenden** verarbeitet Änderungen, bevor **OK** das Ergebnis
auf alle Instanzen dieses Gerätetyps anwendet.

**Download** fordert die Datei aus dem Repository an, wenn das Modell eindeutig
übereinstimmt, auch bei abweichenden Modusnamen. `DMX Mode` verhindert beispielsweise
keinen ELP-PAR-Download mit den Modi `Basic` und `Compact`. Bestätigen Sie nach dem
Download den tatsächlichen Modus, wenn seine Kompatibilität geprüft werden muss.
Bei Abbruch der Moduswahl bleibt das Paket mit **Heruntergeladen; DMX-Modus wählen**
erhalten. Verwenden Sie später **DMX-Modus wählen...** oder wählen Sie eine andere
Quelle. OK bleibt bei ausstehenden Modusentscheidungen deaktiviert.

Jede Zeile bietet die exklusiven Kontrollkästchen **MVR**, **Perastage** und
**Ausgewählte Datei**. Nicht verfügbare Quellen sind deaktiviert. MVR erhält die
importierten Modi; Perastage verwendet ein verfügbares Bibliotheksprofil;
Ausgewählte Datei stellt die letzte heruntergeladene oder lokale Auswahl wieder her.
Ein erneuter Wechsel zur heruntergeladenen Datei verwendet das geprüfte Paket.
**Heruntergeladen; identisch mit ursprünglichem MVR** bedeutet, dass die Dateibytes
vollständig verglichen wurden und identisch sind. Gleiche Namen oder FixtureTypeIDs
beweisen keine Dateiidentität.

**GDTF Share durchsuchen...** verwendet den bestehenden Katalogbrowser.
**Lokale GDTF laden...** prüft die Datei und kopiert sie in die Importressourcen,
ohne das Original zu ändern. Wählen Sie den DMX-Modus und prüfen Sie Unterschiede
bei Modell, Modus und Kanalzahl. Verfügbare Ersatzquellen lassen sich in derselben
Zeile ausdrücklich auswählen.

Das automatische Schließen wird ausschließlich unter **Einstellungen → MVR Import/Export**
konfiguriert und bleibt standardmäßig aus. Wenn aktiviert, setzt es den Import mit
sicheren abgeschlossenen Auswahlen fort. Mehrdeutige Ergebnisse und Fehler bleiben
zur Prüfung offen.

Die Warteschlange zeigt Katalog- und Anmeldeaktivität, verstrichene Zeit, Fortschritt
pro Gerätetyp und heruntergeladene Bytes. Grün bedeutet erfolgreicher Download,
Wiederverwendung oder lokale Auswahl; Gelb bedeutet originales MVR-Profil oder
Ersatzprofil der Anwendung; Rot bedeutet Fehler oder nicht aufgelöstes Profil.

Kandidaten zeigen alle verfügbaren Modi und Kanalzahlen. Fehlende Katalogdaten
erscheinen als nicht verfügbar statt als null Kanäle und werden nach dem Download
aus dem Paket ergänzt. Die Details zeigen den importierten Modus und seine bekannte
Kanalzahl. Wiederholte Patch-Abstände können einen Modus vorschlagen, erfordern
aber Bestätigung: Abstände können freie Adressen enthalten, und gleiche Kanalzahlen
garantieren keine gleichen DMX-Funktionen.

**Abbrechen**, **Escape** und das Schließen des Fensters stoppen die Warteschlange
und setzen den Import für alle ihre Zeilen mit den ursprünglichen MVR-Profilen
und Modi fort. Sämtliche Ersetzungen dieser Warteschlange werden verworfen, auch
abgeschlossene Downloads und lokale Auswahlen. **OK** übernimmt die Ergebnisse.

**Auswahl anwenden** wiederholt auch fehlgeschlagene Downloads. Ein Abbruch der Anmeldung beendet die übrigen Versuche, ohne den Anmeldedialog für jede Zeile erneut zu öffnen.
