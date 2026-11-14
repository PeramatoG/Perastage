# Mises en page et PDF

Les mises en page organisent les vues de scène et les informations complémentaires sur des pages imprimables.

## Préparer une page

1. Ouvrez **View → Layout Views → Layout Mode View**. Affichez le panneau **Layouts** depuis **View** si nécessaire.
2. Sélectionnez une mise en page ou créez-en une avec **Add**. **Rename** et **Delete** gèrent les pages ; la dernière ne peut pas être supprimée.
3. Choisissez l’orientation dans le menu contextuel.
4. Ajoutez des éléments depuis la barre Layout :

| Action | Contenu |
| --- | --- |
| **Add 2D View** | Vue de scène 2D |
| **Add Legend** | Légende de projecteurs |
| **Add Event Table** | Informations sur l’événement |
| **Add Text** | Notes et titres |
| **Add Image** | Images, par exemple logos |

Modifiez les réglages disponibles de chaque élément pour choisir le contenu et la présentation. Vérifiez les données dans les [vues et tables](views.md) avant de produire la documentation.

## Organiser et modifier

Glissez un cadre pour le déplacer et une poignée pour le redimensionner. Double-cliquez le cadre pour modifier l’élément. Le clic droit propose édition, suppression et ordre de superposition. Glissez dans l’espace vide pour déplacer la vue et utilisez la molette pour zoomer. **Z** ajuste la mise en page ; **Delete** supprime l’élément sélectionné.

Enregistrez le projet `.pstg` pour conserver les mises en page. **Export template** crée une archive portable `.pslayout` avec les images référencées. **Import template** accepte `.pslayout` et les anciens modèles `.json`.

## Exporter en PDF

Sélectionnez une mise en page non vide et choisissez **File → Print Layout...**. Choisissez une destination `.pdf` accessible en écriture. Le PDF utilise le format et l’orientation sélectionnés et inclut les éléments pris en charge. Vérifiez le fichier avant impression ou partage.

Pour les vues manquantes, sorties incorrectes ou erreurs d’export, consultez [Dépannage](troubleshooting.md#problèmes-de-mise-en-page-et-de-pdf).

## Échelle physique et propriétés de la vue

Double-cliquez sur une vue 2D puis ouvrez **Propriétés de la vue**. **Libre / Variable** reste le mode par défaut des nouvelles vues et anciens fichiers, avec zoom, déplacement et cadrage visuel. L’échelle effective approximative est informative. **Cadrage visuel** règle le cadrage visuel, pas un rapport physique.

Choisissez **Fixe** pour 1:25, 1:50, 1:100, 1:200, 1:500 ou un dénominateur personnalisé entre 0,01 et 10000000. Un mètre mesure 20 mm sur papier à 1:50 et 10 mm à 1:100. Le centre utilise des mètres et les axes dessus X/Y, face X/Z ou côté Y/Z ; en vue de côté, l’axe horizontal affiché est −Y. Saisissez le centre ou déplacez directement la vue dans l’éditeur. En mode fixe, le rectangle rouge montre le recadrage réel à l’échelle choisie. Le zoom, l’ajustement automatique et le curseur de cadrage sont verrouillés ; le déplacement et les flèches changent le centre sans modifier l’échelle. Le mode libre restaure le zoom précédent et conserve le dernier centre.

Les changements de mode conservent le centre actuel dans les deux sens. Un déplacement ou une modification du centre en mode fixe reste appliqué lorsque le verrouillage est retiré et que le zoom libre précédent est rétabli. Les deux modes utilisent le rendu 2D habituel pour l’aperçu du Layout, avec le même dessin de la scène et des symboles.

Les millimètres sont affichés sans décimales. Les fractions saisies et la précision interne des champs inchangés sont conservées.

Les flèches ajustent les dimensions du cadre par pas de 1 mm, le centre par pas de 0,01 m et le dénominateur de l’échelle par pas de 1, en conservant les fractions internes. La saisie manuelle et les échelles courantes restent disponibles. En mode fixe, un indicateur visible affiche le rapport et explique le verrouillage du zoom.

Les modifications valides dans **Propriétés de la vue** sont immédiatement prévisualisées dans l’éditeur : mode, échelle, centre et dimensions du cadre. Une valeur incomplète ou non valide conserve le dernier aperçu valide. **Cancel** ou la fermeture du dialogue restaure les propriétés et le cadrage précédents ; **OK** conserve les modifications dans le brouillon de l’éditeur.

Les dimensions du cadre utilisent des millimètres et conservent la précision fractionnaire. Déplacez l’élément sur la page du Layout pour changer sa position. Les placements partiellement hors page restent permis. Le minimum est 1 point (0,353 mm), la limite 200000 points. Déplacer ou redimensionner un cadre fixe conserve échelle et centre et change la zone visible. Le zoom du Layout et les DPI n’affectent que l’affichage. Revenir au mode libre conserve le zoom précédent, le dernier centre, les filtres et la présentation. **Cancel** abandonne le brouillon ; **OK** valide avec Annuler/Rétablir et sauvegarde PSTG/PSLAYOUT.

Le menu contextuel de page propose A3/A4 et l’orientation sans redimensionner le contenu. Les vues fixes exigent un PDF à 100 % au même format que le Layout ; un autre format est refusé avec une explication. Pour changer de papier, modifiez d’abord celui du Layout. Les pages libres conservent l’ajustement uniforme. Imprimez le PDF à taille réelle/100 % : Perastage ne contrôle pas la mise à l’échelle du pilote. Le contrat physique porte sur la géométrie et n’élargit pas les garanties typographiques existantes.
