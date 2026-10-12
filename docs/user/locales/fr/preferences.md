# Préférences

Ouvrez **Edit → Preferences**. Changer de catégorie conserve les modifications en attente. **Apply** enregistre sans fermer, **OK** enregistre et ferme, et **Cancel** annule les changements depuis le dernier Apply.

## Général

- **Language:** choisissez la langue de l’interface ; redémarrez Perastage pour l’appliquer.
- **Units:** choisissez les unités de distance et de poids. Elles changent immédiatement.
- **Updates:** **Check on startup (recommended)** vérifie au plus une fois toutes les 24 heures. Vous pouvez supprimer les rappels d’une version. **Manual only** désactive la vérification au démarrage ; **Help → Check for Updates** reste disponible à la demande.

### Cache

**General → Cache** affiche l’emplacement du cache en lecture seule et l’espace utilisé. **Open Cache Folder** ouvre le dossier dans le gestionnaire de fichiers. **Clear Cache...** demande confirmation et supprime immédiatement les caches Perastage régénérables, indépendamment de Apply, OK ou Cancel. La géométrie est reconstruite si nécessaire ; le catalogue GDTF est téléchargé de nouveau en ligne. Configuration, journaux, identifiants, bibliothèques, GDTF, dictionnaires et projets sont conservés. Si des fichiers ne peuvent pas être supprimés, Perastage signale l’échec et actualise la taille affichée. Les caches persistants se trouvent dans `cache/`, sous le dossier de données de l’utilisateur, et non près des ressources source.

## Import

**Rider Import** contrôle **Auto patch after import**, les calques automatiques par position ou type de projecteur et les hauteurs, positions et marges LX par défaut. Les champs de distance suivent les unités choisies. Consultez [Créer à partir de texte](create-from-text.md).

## Vue

### Vue 3D

Choisissez la présentation et **Performance → Model detail** :

- **Standard** (par défaut) équilibre détails et performances.
- **Low** simplifie davantage les modèles lourds.
- **High** affiche tous les détails d’origine disponibles.

**Use a simplified proxy while navigating** est indépendant et désactivé par défaut. Il simplifie temporairement les projecteurs non sélectionnés lors d’une navigation rapide, puis restaure le niveau choisi. Les changements s’appliquent au prochain rendu sans rouvrir le projet. La géométrie source, les ressources GDTF, symboles 2D et géométries exportées restent inchangés.

### Sélection et déplacement

Choisissez les types d’objets qui déplacent leur groupe racine pendant les transformations souris, Console et aimant. Par défaut, les structures groupées déplacent leur groupe ; les projecteurs, supports/palans et objets de scène se déplacent indépendamment. Sélectionner directement un groupe le déplace. Les modifications de table ne touchent que l’objet modifié.

**Show anchor references while moving or inserting elements** affiche les ancrages compatibles de Magnet pendant le déplacement ou l’insertion. Ces guides ne font pas partie de la scène ni de l’export. Changer ces préférences ne modifie pas la hiérarchie du projet et ne le marque pas comme modifié.

## Formats

### Ressources GDTF

Les préférences GDTF gèrent les identifiants Share. Perastage stocke automatiquement les symboles Top/Front/Side/Bottom du projecteur dans PSTG. Enregistrez le projet pour conserver les symboles générés et remplacements utilisateur ; ils ne deviennent pas des ressources GDTF ou MVR. Le Fixture Editor inspecte les SVG d’origine par Model et conserve leurs octets et décalages. Les modifications explicites de propriétés physiques restent disponibles.

### Import / export MVR

**Truss geometry export mode** propose :

- **Standard MVR representation** (par défaut) : conserver les références de symboles importées si possible.
- **Direct Geometry3D for truss symbols:** développer ces références pour les applications nécessitant une géométrie directe.

Les deux produisent un MVR compatible 1.6. L’enregistrement du projet conserve la représentation standard. Les choix d’import sont décrits dans [Ouvrir des fichiers MVR](opening-mvr-files.md).
