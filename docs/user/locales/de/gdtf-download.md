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

**GDTF Share durchsuchen...** verwendet den bestehenden Katalogbrowser.
**Lokale GDTF laden...** prüft die Datei und kopiert sie in die Importressourcen,
ohne das Original zu ändern. Wählen Sie den DMX-Modus und prüfen Sie Unterschiede
bei Modell, Modus und Kanalzahl. Verfügbare Ersatzquellen lassen sich in derselben
Zeile ausdrücklich auswählen.

Das automatische Schließen bleibt in den MVR-Einstellungen standardmäßig aus und
kann auch in der Warteschlange geändert werden. Wenn aktiviert, setzt es den
Import mit sicheren abgeschlossenen Auswahlen fort. Mehrdeutige Ergebnisse und
Fehler bleiben zur Prüfung offen. Abbrechen erhält abgeschlossene Downloads und
gültige Ersatzprofile. Die gleiche Kanalzahl garantiert keine gleiche DMX-Funktion.
