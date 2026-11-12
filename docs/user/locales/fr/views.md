# Vues

Utilisez la 3D pour vérifier les relations spatiales et orientations, et la 2D pour les plans, positions et lisibilité. Affichez les volets masqués depuis **View**.

## Naviguer et vérifier

En 3D, glissez pour orbiter, Maj-glissez ou glissez avec le bouton central pour déplacer la caméra, et utilisez la molette pour zoomer. Le clic droit puis **Render style** change la présentation, notamment les couleurs par type, calque ou univers.

En 2D, glissez dans l’espace vide pour déplacer la vue et utilisez la molette pour zoomer. Sélectionnez des éléments pour vérifier leurs lignes dans Fixtures, Trusses, Hoists et Objects. Double-cliquez une cellule d’une sélection multiple pour modifier la colonne des lignes sélectionnées.

## Outil de position

Ouvrez **View → Position Tool** dans l’espace de travail 2D ou 3D. Le même panneau peut être ancré, flottant, redimensionné ou masqué ; sa position et sa visibilité sont enregistrées avec l’espace de travail. Sans sélection, les champs sont désactivés.

Les champs X, Y et Z partagent une seule ligne de largeur flexible dans un panneau de hauteur réduite.

Le panneau apparaît par défaut sous Summary dans la colonne droite en 2D et 3D. Les flèches de chaque axe déplacent chaque cible effective de 1 mm par clic en métrique ou de 0,01 pied en impérial, en conservant les différences et la précision enregistrée. Chaque clic peut être annulé.

X, Y et Z sont les coordonnées absolues du point d’insertion dans le monde. Modifiez un champ puis appuyez sur Entrée ou quittez le champ pour appliquer la valeur ; les autres axes, la rotation et l’échelle sont conservés. Échap annule la modification en cours. Chaque modification validée constitue une étape d’annulation. Une saisie vide ou incorrecte ne déplace rien.

Pour une sélection multiple, les coordonnées communes sont affichées et les coordonnées différentes restent vides. Une valeur saisie affecte cette coordonnée absolue à chaque cible effective séparément. Les enfants groupés suivent les préférences de **Selection & Movement**.

De petits carrés rouges indiquent les points d’insertion effectifs dans les vues Dessus, Dessous, Face et Côté du visualiseur 2D interactif. Leur taille à l’écran reste constante et ils sont exclus des captures, des mises en page imprimées et des exports. Aucun indicateur n’est affiché en 3D.

Les unités suivent **Preferences → Units** : mètres avec trois décimales ou format impérial existant en pieds. Les suffixes d’unité et la saisie en pieds/pouces sont acceptés. L’arrondi d’affichage ne modifie pas les positions enregistrées.

## Déplacer et sélectionner

- **Drag Move** active le glissement des sélections avec le bouton gauche ; il est désactivé par défaut.
- **Axis Lock** contraint le déplacement. Le désactiver permet un déplacement libre en 2D ou dans un plan face à la caméra en 3D.
- **Local Axes** change les axes de transformation de la vue.
- **Magnet** accroche les ancrages compatibles pendant le déplacement ou l’insertion.
- **Cross-table Actions** permet de cibler des types d’objets hors de la table active ; il est désactivé par défaut.

Le déplacement groupé suit les [préférences de sélection et déplacement](preferences.md#sélection-et-déplacement). Vérifiez la position finale dans les deux vues et utilisez Undo si nécessaire.

Consultez [Raccourcis et barre de commandes](shortcuts-and-command-bar.md) pour les entrées clavier, souris et Console. Pour les pages imprimables, utilisez [Mises en page et PDF](layouts.md).
