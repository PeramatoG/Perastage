# Ouvrir des fichiers MVR

## Ouvrir ou importer

Utilisez **File → Import MVR...** et choisissez :

- **Open as new project** pour remplacer la scène après les vérifications habituelles du projet non enregistré.
- **Merge into current project** pour ajouter du contenu tout en conservant votre travail.

Ouvrir directement un `.mvr` au démarrage ou par une association de fichiers prise en charge le charge comme nouveau projet. Annuler le choix d’import laisse le projet actuel inchangé.

Vérifiez la scène importée en [2D/3D](views.md) et dans les tables Fixtures, Trusses, Hoists et Objects. Utilisez l’[Inspecteur](inspector.md) pour examiner une archive sans l’importer.

## Conflits de fusion

En cas de collision d’UUID, choisissez de créer de nouveaux UUID pour les objets entrants (choix par défaut, conservant les deux scènes), de remplacer les objets correspondants du projet ou d’ignorer les objets entrants en conflit.

Si un nom de type de projecteur désigne une définition ou un mode GDTF différent, choisissez la définition du projet actuel, conservez la définition importée sous un type renommé, ou annulez la fusion. Une fusion annulée ou échouée restaure le projet actuel.

Les doublons d’adresses DMX produisent des avertissements non bloquants dans la Console. Vérifiez et corrigez le patch après la fusion.

## Ressources et stockage du projet

Les GDTF, symboles et modèles importés sont conservés avec les ressources du projet afin de rester accessibles après enregistrement et réouverture. Vérifiez les profils et modes manquants avant l’export ; consultez [Téléchargement GDTF](gdtf-download.md) et [Dépannage](troubleshooting.md).

Enregistrez le travail en cours en `.pstg`, qui conserve le contexte Perastage, les mises en page et les symboles du projet. MVR est le format d’échange. Perastage stocke ses métadonnées d’échange dans un UserData propre à l’application et conforme au standard ; d’autres applications peuvent ne pas les conserver.

## Exporter MVR

1. Choisissez **File → Export MVR...**.
2. Choisissez une destination ; le nom proposé utilise celui du projet s’il est disponible.
3. Exportez et vérifiez le résultat dans l’application destinataire.

Les [Préférences](preferences.md#import--export-mvr) proposent une option de compatibilité pour la géométrie des structures. Les symboles du projet enregistrés dans PSTG ne sont pas exportés vers MVR ni intégrés aux GDTF.
