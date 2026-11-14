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

## Aligner les objets de scène

Utilisez la barre **Alignement** dans l’espace 2D ou 3D. Sélectionnez au moins deux cibles effectives dans les vues ou les tableaux. Choisissez **Étendue**, **Premier sélectionné** ou **Dernier sélectionné**, puis **Aligner en haut**, **Aligner en bas**, **Aligner à gauche**, **Aligner à droite**, **Centrer horizontalement** ou **Centrer verticalement**. La référence premier/dernier suit l’ordre de sélection entre types et reste fixe. Retirer puis sélectionner à nouveau un objet le place en dernier. Les ajouts groupés suivent l’ordre stable des UUID ; trier les tableaux conserve l’ordre précédent.

La référence et le plan utilisent deux groupes d’icônes exclusifs, avec une option active dans chaque groupe. Survolez une icône pour afficher son aide. En 2D, le plan suit automatiquement la vue : **XY** pour Dessus, **XZ** pour Face et **YZ** pour Côté. En 3D, les boutons **XY/XZ/YZ** partagent le plan entre alignement et déplacement des sélections. Les vues standard actualisent le plan ; orbiter en perspective le conserve. Le plan initial est XY. L’alignement conserve la troisième coordonnée, la rotation et l’échelle ; la vue de côté inverse Y. Le verrouillage des axes choisit une direction dans le plan ; les axes locaux sont projetés sur celui-ci. Le déplacement libre utilise le plan passant par la sélection. Si le plan est vu par la tranche, changez de vue ou orbitez. Magnet est une exception : l’accrochage peut sortir du plan pour atteindre un élément proche. Le placement continu et la distribution conservent leur comportement. Les groupes suivent les préférences de déplacement ; leurs descendants sélectionnés ne sont jamais déplacés deux fois.

Un alignement modifié crée une étape Annuler ; le répéter sans changement ne crée pas d’historique. Une géométrie physique manquante annule toute l’opération ; les diagnostics identifient l’UUID/la ressource. Les palans fictifs sans géométrie physique ne peuvent pas être alignés par étendue. L’alignement Layout n’est pas inclus.
