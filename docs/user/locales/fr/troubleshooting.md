# Dépannage

## Problèmes d’ouverture et d’import MVR

Vérifiez l’extension `.mvr`, essayez un autre fichier pour isoler le problème et consultez les messages d’import dans la Console. Utilisez l’[Inspecteur](inspector.md) pour examiner une archive externe sans l’importer. Pour les conflits de fusion, consultez [Ouvrir des fichiers MVR](opening-mvr-files.md).

## Projecteurs GDTF manquants ou incorrects

Vérifiez la définition et le mode DMX sélectionné. Utilisez **Tools → Download GDTF** pour les profils et **Tools → Edit dictionaries** pour les correspondances locales. Rouvrez et vérifiez la scène après correction des profils manquants. Examinez le Model sélectionné dans le Fixture Editor ; les symboles du projet sont stockés séparément dans PSTG.

## Bibliothèque utilisateur

Utilisez **Tools → Open user library folder** ; sous Windows, le chemin par défaut est `%APPDATA%\Perastage\library\`. Vérifiez que votre profil est accessible en écriture et sauvegardez les contenus personnalisés avant remplacement. Modifiez les fichiers utilisateur plutôt que les valeurs installées.

## Problèmes de mise en page et de PDF

Sélectionnez une mise en page non vide, vérifiez ses cadres et orientation ainsi que les données de scène et tables avant **File → Print Layout...**. Choisissez une destination accessible en écriture et un nom non vide. Vérifiez le PDF ; consultez [Mises en page et PDF](layouts.md).

## Problèmes de projet et d’exécution

Enregistrez si possible une copie distincte du projet, redémarrez Perastage et rouvrez-la. Vérifiez les données critiques en 2D, 3D et dans les tables. Si la navigation 3D est lente, réduisez [le détail des modèles](preferences.md#vue-3d) ou activez la représentation simplifiée de navigation. Comparez avec un petit projet pour isoler les problèmes propres à la scène.

## Identifiants GDTF Share

Les chargements en ligne et téléchargements nécessitent compte et accès réseau ; la [consultation du catalogue en cache](gdtf-download.md#navigation-en-cache-et-hors-ligne) reste possible hors ligne.

Si Perastage signale l’indisponibilité du stockage sécurisé, le mot de passe reste utilisable pour l’opération actuelle mais n’est pas conservé. Vérifiez le stockage du système ; Linux nécessite un fournisseur Secret Service actif, comme GNOME Keyring ou KWallet. Si l’avertissement persiste dans un paquet publié, incluez-le dans votre rapport. N’incluez aucun mot de passe.

## Journaux et rapports de diagnostic

Utilisez **Help → Open Logs Folder** pour trouver `perastage.log`, le journal du lancement précédent `perastage.previous.log` et le sous-dossier `crash_reports`.

Emplacements par défaut :

- Windows: `%LOCALAPPDATA%\Perastage\logs\`
- macOS: `~/Library/Logs/Perastage/`
- Linux: `${XDG_STATE_HOME}/perastage/logs/`, ou `~/.local/state/perastage/logs/` si non défini.

Utilisez **Help → Export Diagnostic Report...** pour un rapport texte avec les informations de version et plateforme, les données OpenGL disponibles et les lignes récentes du journal. Les rapports restent locaux et ne sont pas envoyés automatiquement. Examinez-les avant partage et joignez les étapes de reproduction ainsi qu’un petit exemple si possible.

Les erreurs de compilation, dépendances et chaîne d’outils relèvent du [guide développeur](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md).
