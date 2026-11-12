# Présentation des fonctions

Perastage est un outil de bureau libre pour les scènes d’éclairage et d’accroche et leur documentation technique.

## Travail sur les projets MVR

[Ouvrez, fusionnez et exportez des MVR](opening-mvr-files.md), examinez les conflits et enregistrez le travail en cours comme projet PSTG.

## GDTF et GDTF Share

Utilisez les définitions de projecteurs et dictionnaires locaux, [recherchez et téléchargez des GDTF](gdtf-download.md), consultez le catalogue en cache hors ligne et examinez révisions et modes DMX. Le Fixture Editor inspecte les Models et préserve les ressources GDTF d’origine.

## Inspecteur MVR / GDTF

[Inspectez](inspector.md) les instantanés et archives externes sans les modifier. Parcourez les arborescences de scène et d’archive, définitions, XML, aperçus et problèmes signalés.

## Visualisation 2D et 3D

[Vérifiez les plans et relations spatiales](views.md), choisissez les styles de rendu et utilisez les contrôles de déplacement, d’axes et d’accrochage. Adaptez les détails des modèles aux grandes scènes.

## Édition de scène, tables, patch et groupes

Modifiez projecteurs, structures, palans et objets ; vérifiez métadonnées et patch ; organisez calques et groupes. Utilisez les tables, le placement continu, Undo/Redo et la [Console](shortcuts-and-command-bar.md) intégrée.

## Créer à partir de texte

[Générez des scènes initiales](create-from-text.md) depuis du texte collé ou des fiches TXT/PDF, vérifiez les correspondances de types, choisissez les modes ou poursuivez avec un profil générique.

## Mises en page et documentation PDF

[Organisez des pages imprimables](layouts.md) avec vues 2D, légendes, tables d’événement, textes et images. Partagez des modèles de mise en page et exportez en PDF.

## Intégration locale expérimentale

OSC et [MCP](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/mcp_adapter.md) proposent une intégration locale expérimentale pour des automatisations configurées séparément. Les contrats techniques figurent dans la [documentation développeur](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/index.md).

## Plateformes et sources

Perastage prend en charge Windows, macOS et Linux sous GNU GPL version 3 ou ultérieure. Consultez [Installation](installation.md) pour les paquets et le [dépôt](https://github.com/PeramatoG/Perastage) pour les sources et contributions.

## Patch DMX manuel

La saisie manuelle pour un appareil conserve un départ Universe/Channel valide (canal 1–512), même si son empreinte GDTF primaire dépasse cet univers. Une cellule Channel rouge explique au survol le dépassement, une empreinte inconnue ou un chevauchement. Par exemple, 180 canaux à 1.400 couvriraient 1.400–1.512 et 2.1–2.67 avec un Auto Span continu hypothétique. Ces plages sont informatives : MVR standard conserve le départ sans garantir ce routage, qui doit être pris en charge par le contrôleur ou le récepteur. Les breaks secondaires sont conservés. AutoPatch et le patch séquentiel multiple gardent leurs règles de changement d’univers.

## Vue DMX Patch

Ouvrez **Affichage → DMX Patch** pour consulter les 512 canaux de chaque univers sous la vue 2D/3D active. Les cellules compactes et le nombre de colonnes s’adaptent automatiquement à la largeur disponible ; saisissez un numéro pour afficher un univers vide. Les couleurs suivent celles des appareils et les libellés montrent les attributs physiques GDTF ; les infobulles identifient tous les occupants et breaks. Les cellules hachurées indiquent les chevauchements. La liste des diagnostics explique les définitions manquantes, les affectations et les dépassements ; aucun canal ne continue automatiquement dans l’univers suivant. Cette vue est en lecture seule. Fermez-la pour libérer de la place ; sa visibilité et sa hauteur sont mémorisées. Elle est temporairement masquée dans les modes Layout et Inspector. Utilisez les boutons de titre pour fermer ou agrandir le panneau. L’icône d’avertissement ouvre une liste déroulante des diagnostics au lieu d’une longue infobulle.

Le nom de chaque appareil apparaît uniquement sur sa première ligne ; les numéros centrés, le premier canal encadré et les coins arrondis indiquent ses limites. **Offsets déclarés** est activé par défaut pour les définitions ambiguës : les octets provisoires sont affichés depuis le départ primaire, avec bordures en pointillés, points d’interrogation et un compteur distinct de l’occupation et des conflits confirmés. Cette option ne répare pas les breaks, ne réécrit aucun fichier et ne prolonge pas les canaux dans un autre univers. Les contours de sélection et de survol restent synchronisés avec la table et les vues, en reprenant le cyan de sélection et les verts de survol de la table sans changer les couleurs de remplissage. Les noms sont en gras. Cliquez sur un canal pour sélectionner ses appareils dans la table et les deux vues ; Ctrl ajoute et Maj inverse la sélection. Un clic simple sur un canal libre efface la sélection. Les préférences de promotion au groupe s’appliquent ; un canal superposé sélectionne tous ses appareils. La sélection ne change jamais les adresses DMX. Survoler un canal du patch ou une ligne de la table met le même appareil en évidence dans le patch, la table et les vues ; quitter efface le survol et conserve le cyan de sélection.
