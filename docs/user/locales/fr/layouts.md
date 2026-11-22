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

Cliquez sur un cadre pour le sélectionner. **Ctrl + clic** inverse sa sélection
et **Maj + clic** ajoute un élément. **Maj + glisser** sur un espace vide ajoute
les cadres qui croisent le rectangle, de l’arrière vers l’avant. À profondeur
égale, l’ordre est Vue 2D, Légende, Tableau d’événement, Texte, Image, puis l’ordre
stocké. Un clic choisit le cadre au premier plan. Le dernier élément sélectionné
restant est principal (contour plus foncé). Retirer puis sélectionner à nouveau
un élément le place en dernier. La sélection est indépendante de la scène et
s’efface au changement de page.

Faites glisser un cadre sélectionné pour déplacer toute la sélection sans modifier
les dimensions ni les positions relatives. Les poignées de redimensionnement
s’affichent uniquement pour un élément seul. Double-cliquez sur un cadre pour
modifier uniquement cet élément. **Suppr** retire toute la sélection en une étape
d’annulation. Le menu contextuel d’une sélection multiple propose la suppression
groupée. Cliquez dans le vide pour effacer la sélection ; glissez sans modificateur
pour déplacer la page. La molette règle le zoom et **Z** ajuste la page.

La barre partagée **Alignement** accepte des sélections mixtes de deux éléments
ou plus. Choisissez Étendue, Premier sélectionné ou Dernier sélectionné, puis
l’une des six opérations de bord ou de centrage. Premier et Dernier suivent
l’ordre de sélection et laissent le cadre de référence immobile. Les limites
géométriques utilisent les cadres entiers ; les points d’insertion désignent
les coins supérieurs gauches des cadres sur la page, jamais ceux des projecteurs
dans une vue. Les plans de travail sont désactivés en mode Layout.

Les vues conservent leur précision fractionnaire. Les autres éléments arrondissent
la position au point le plus proche, les demi-points allant vers l’infini positif, avec
une erreur maximale de 0,5 pt par axe. Un déplacement mixte arrondit le décalage
commun aux points entiers pour conserver les positions relatives. L’alignement
sur l’étendue d’une sélection contenant des cadres entiers s’arrête à 0,5 pt
près pour éviter une dérive lors des répétitions. Échelles, caméras,
filtres et positions de scène sont conservés. Les contours de sélection ne sont pas imprimés.

Enregistrez le projet `.pstg` pour conserver les mises en page. **Export template** crée une archive portable `.pslayout` avec les images référencées. **Import template** accepte `.pslayout` et les anciens modèles `.json`.

## Exporter en PDF

Sélectionnez une mise en page non vide et choisissez **File → Print Layout...**. Choisissez une destination `.pdf` accessible en écriture. Le PDF utilise le format et l’orientation sélectionnés et inclut les éléments pris en charge. Vérifiez le fichier avant impression ou partage.

Pour les vues manquantes, sorties incorrectes ou erreurs d’export, consultez [Dépannage](troubleshooting.md#problèmes-de-mise-en-page-et-de-pdf).

## Typographie du document

Utilisez **Famille de police** pour choisir PerastageSans, Noto Serif, Source Sans 3 ou Noto Sans Mono dans les étiquettes, les légendes, les tableaux d’événement et les textes. Dans l’éditeur de texte, sélectionnez un passage pour modifier sa police ou laissez la sélection vide pour modifier tout l’élément. Le PDF intègre les polices approuvées et permet de sélectionner les caractères pris en charge. Utilisez **Aperçu du document** pour vérifier le rendu de la mise en page. **Actualiser les polices** vérifie à nouveau les fichiers approuvés.

Lorsqu’une police manque, Perastage conserve le choix enregistré, utilise une police de remplacement approuvée et affiche un avertissement dans les détails de l’exportation. Rouvrez le projet après avoir restauré la police.

## Échelle physique et propriétés de la vue

Double-cliquez sur une vue 2D puis ouvrez **Propriétés de la vue**. **Libre / Variable** reste le mode par défaut des nouvelles vues et anciens fichiers, avec zoom, déplacement et cadrage visuel. L’échelle effective approximative est informative. **Cadrage visuel** règle le cadrage visuel, pas un rapport physique.

Choisissez **Fixe** pour 1:25, 1:50, 1:100, 1:200, 1:500 ou un dénominateur personnalisé entre 0,01 et 10000000. Un mètre mesure 20 mm sur papier à 1:50 et 10 mm à 1:100. Le centre utilise des mètres et les axes dessus X/Y, face X/Z ou côté Y/Z ; en vue de côté, l’axe horizontal affiché est −Y. Saisissez le centre ou déplacez directement la vue dans l’éditeur. En mode fixe, le rectangle rouge montre le recadrage réel à l’échelle choisie. Le zoom, l’ajustement automatique et le curseur de cadrage sont verrouillés ; le déplacement et les flèches changent le centre sans modifier l’échelle. Le mode libre restaure le zoom précédent et conserve le dernier centre.

Les changements de mode conservent le centre actuel dans les deux sens. Un déplacement ou une modification du centre en mode fixe reste appliqué lorsque le verrouillage est retiré et que le zoom libre précédent est rétabli. Les deux modes utilisent le rendu 2D habituel pour l’aperçu du Layout, avec le même dessin de la scène et des symboles.

Les millimètres sont affichés sans décimales. Les fractions saisies et la précision interne des champs inchangés sont conservées.

Les flèches ajustent les dimensions du cadre par pas de 1 mm, le centre par pas de 0,01 m et le dénominateur de l’échelle par pas de 1, en conservant les fractions internes. La saisie manuelle et les échelles courantes restent disponibles. En mode fixe, un indicateur visible affiche le rapport et explique le verrouillage du zoom.

Les modifications valides dans **Propriétés de la vue** sont immédiatement prévisualisées dans l’éditeur : mode, échelle, centre et dimensions du cadre. Une valeur incomplète ou non valide conserve le dernier aperçu valide. **Cancel** ou la fermeture du dialogue restaure les propriétés et le cadrage précédents ; **OK** conserve les modifications dans le brouillon de l’éditeur.

Les dimensions du cadre utilisent des millimètres et conservent la précision fractionnaire. Déplacez l’élément sur la page du Layout pour changer sa position. Les placements partiellement hors page restent permis. Le minimum est 1 point (0,353 mm), la limite 200000 points. Déplacer ou redimensionner un cadre fixe conserve échelle et centre et change la zone visible. Le zoom du Layout et les DPI n’affectent que l’affichage. Revenir au mode libre conserve le zoom précédent, le dernier centre, les filtres et la présentation. **Cancel** abandonne le brouillon ; **OK** valide avec Annuler/Rétablir et sauvegarde PSTG/PSLAYOUT.

Le menu contextuel de page propose A3/A4 et l’orientation sans redimensionner le contenu. Les vues fixes exigent un PDF à 100 % au même format que le Layout ; un autre format est refusé avec une explication. Pour changer de papier, modifiez d’abord celui du Layout. Les pages libres conservent l’ajustement uniforme. Imprimez le PDF à taille réelle/100 % : Perastage ne contrôle pas la mise à l’échelle du pilote. Le contrat physique porte sur la géométrie et n’élargit pas les garanties typographiques existantes.

## Styles réutilisables des étiquettes des appareils

Cliquez avec le bouton droit sur le cadre d’une vue 2D et choisissez **Réglages des
étiquettes des appareils...**. Héritez de la présentation du projet ou sélectionnez
un style nommé. Vous pouvez créer un style depuis les réglages actuels, le modifier,
le renommer ou le dupliquer. Ses modifications concernent toutes les vues qui
l’utilisent, y compris sur d’autres pages. Un style référencé ne peut pas être supprimé :
rétablissez d’abord les réglages hérités dans les vues concernées.

**Personnaliser la vue actuelle...** ouvre l’éditeur existant pour la vue actuelle
ou ses appareils sélectionnés. Sélectionnez les appareils dans la scène au préalable.
L’aperçu, son fond et le déplacement des champs restent disponibles. Chaque champ
modifié d’une orientation enregistre ses propriétés complètes ; les autres héritent.
Priorité : appareil local, champ de vue, champ du style, appareil du projet, valeurs
du projet, puis configuration héritée. Réinitialiser le réglage local conserve les
personnalisations du projet. Rétablir les réglages hérités dans le dialogue de vue
efface aussi l’affectation du style.

Dans l’éditeur imbriqué, Appliquer conserve le brouillon et OK le transmet au dialogue
extérieur. Seul OK dans ce dernier publie une opération Annuler ; Annuler abandonne
les modifications. Sélectionner un style ou un fond ne modifie pas le projet.
PSTG conserve les styles et réglages locaux. PSLAYOUT inclut les styles nécessaires
et remappe les identifiants en conflit de façon déterministe à l’importation.
Une référence absente bloque l’opération avec un diagnostic. MVR et GDTF restent inchangés.
Les tailles et décalages restent en mètres de scène, sans taille papier constante
entre échelles. Bottom conserve la clé de style Side et la projection Top par
compatibilité ; aucune nouvelle orientation SVG standard GDTF n’est créée.

Dans le **2D View Editor**, cliquez sur un appareil pour sélectionner ses étiquettes ; Maj-clic ajoute ou retire des appareils. **Edit Fixture Labels...** ouvre la portée locale sélectionnée. Le glissement déplace toujours la caméra, jamais les appareils. La sélection et l’historique du projet restent inchangés. Le contour et le remplissage des étiquettes à l’écran utilisent la même rastérisation et le même arrondi des pixels, avec l’épaisseur traditionnelle de la scène, y compris à haute résolution. Le PDF conserve ses mesures en mètres de scène.

Toutes les étiquettes des appareils 2D, y compris les champs hérités non modifiés, utilisent les mêmes fichiers Noto Sans, positions des lettres et lignes dans la vue, l’éditeur et le PDF. Ouvrir l’éditeur ne réécrit pas les réglages numériques enregistrés et ne crée pas d’historique. Les étiquettes utilisant auparavant une police système peuvent changer d’aspect pour correspondre à l’éditeur et à l’export. L’aperçu s’adapte à son cadre : le grossissement en pixels peut varier, mais pas les proportions ni l’espacement. Les mesures restent en mètres de scène. Les anciennes captures Layout sont régénérées automatiquement.
