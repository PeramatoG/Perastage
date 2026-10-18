# Installation

Téléchargez depuis le [site officiel](https://perastage.luismaperamato.com/#download), qui propose les paquets par plateforme et les notes de version. Si les métadonnées sont indisponibles, utilisez la [dernière version GitHub](https://github.com/PeramatoG/Perastage/releases/latest). Les paquets disponibles peuvent varier.

## Windows

1. Téléchargez le programme d’installation officiel x64 et exécutez-le.
2. SmartScreen peut signaler une application non reconnue. Vérifiez sa provenance officielle. Si vous lui faites confiance, choisissez **More info → Run anyway**, lorsque cette option est disponible.
3. Terminez l’installation et lancez Perastage depuis le raccourci installé.

Ne désactivez ni Windows Defender ni SmartScreen.

## macOS

1. Choisissez **macOS 15 — Apple Silicon (Compatibility Build)** pour macOS 15 ou **macOS 26 — Apple Silicon** pour macOS 26. Les deux paquets ciblent ARM64 ; les Mac Intel ne sont pas pris en charge.
2. Ouvrez le DMG et glissez Perastage dans Applications.
3. Essayez de lancer Perastage.
4. Les versions actuelles ne sont ni signées Developer ID ni notariées par Apple. Si Gatekeeper bloque l’application car le développeur ne peut pas être vérifié, et si vous faites confiance au téléchargement officiel, utilisez **System Settings → Privacy & Security → Open Anyway**, lorsque disponible.

Consultez les [instructions d’Apple](https://support.apple.com/en-us/102445). Gardez Gatekeeper activé.

### Dépannage avancé de macOS

Certaines versions peuvent indiquer que l’application est endommagée. Téléchargez-la à nouveau depuis la version officielle. Seulement après avoir vérifié sa provenance officielle et fait confiance au fichier, les utilisateurs avancés peuvent exécuter :

```bash
xattr -dr com.apple.quarantine /Applications/Perastage.app
```

Cette commande supprime récursivement la quarantaine de cette application et contourne sa vérification Gatekeeper fondée sur la quarantaine. Elle ne vérifie pas l’application et ne répare pas les fichiers corrompus. Ne l’utilisez pas sur un téléchargement non fiable et ne supprimez pas la quarantaine globalement.

## Linux AppImage

1. Téléchargez l’AppImage x86_64.
2. Dans le dossier de téléchargement, rendez-la exécutable :

```bash
chmod +x Perastage-*.AppImage
```

3. Ouvrez l’AppImage pour lancer Perastage. Aucune installation n’est normalement nécessaire.

## Arch Linux (expérimental)

Le paquet natif x86_64 est expérimental. Dans le dossier du paquet téléchargé, installez-le avec :

```bash
sudo pacman -U ./Perastage-*-arch-x86_64.pkg.tar.zst
```

Lancez ensuite Perastage depuis le menu des applications.

## Votre bibliothèque modifiable

Utilisez **Tools → Open user library folder** pour trouver les dictionnaires, profils de projecteurs et autres contenus modifiables. Sous Windows, le chemin par défaut est `%APPDATA%\Perastage\library\`.

Sauvegardez vos contenus personnalisés. Les éléments fournis complètent les fichiers manquants sans écraser les fichiers existants ; modifiez la copie utilisateur plutôt que le dossier d’installation.

Pour les problèmes de lancement, d’import ou de contenu manquant, consultez [Dépannage](troubleshooting.md). La compilation est décrite dans le [guide développeur](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md).
