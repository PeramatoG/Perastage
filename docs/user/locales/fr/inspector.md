# Inspecteur MVR / GDTF

Ouvrez **View → Layout Views → MVR / GDTF Inspector** pour examiner un instantané du projet actuel. L’inspection est en lecture seule : elle ne modifie ni le projet ni le fichier inspecté, ne change pas la sélection et ne marque pas le projet comme modifié.

## Choisir le contenu à inspecter

- La vue du projet est un instantané, pas une scène actualisée en continu. Utilisez **Refresh** après une modification pour capturer l’état récent.
- Utilisez **Open external...** pour inspecter un `.mvr` ou `.gdtf` externe sans l’importer.

## Parcourir le contenu MVR et GDTF

Pour MVR, parcourez l’arborescence de l’archive ou celle de la scène pour examiner les objets, leurs noms, types et UUID. Activez un GDTF intégré pour ouvrir sa définition de projecteur, puis revenez au MVR parent.

Pour GDTF, examinez l’identité FixtureType, les informations physiques et les révisions. Sélectionnez un mode DMX pour inspecter ses canaux, fonctions et ensembles. Consultez les roues, emplacements, filtres et références de ressources dans les pages détaillées.

## Source et aperçu

Sélectionnez une ressource pour examiner sa source ou son aperçu. Le XML et le texte apparaissent dans **Source** ; SVG propose le XML et un rendu. Les images prises en charge et les modèles GLB/3DS apparaissent dans **Preview**. Les ressources non prises en charge ou trop grandes restent listées avec un état d’aperçu.

Utilisez la recherche, le repli et la copie pour examiner le XML. Les grands documents peuvent commencer par une vue limitée et proposer une action explicite pour charger le texte complet. Déplacez les séparateurs pour agrandir la navigation, la source, les détails ou l’aperçu.

## Problèmes et diagnostics

**Issues** regroupe les problèmes signalés ; **Diagnostics** fournit davantage de détails. Une limite d’aperçu ne signifie pas à elle seule que le fichier est invalide. L’Inspecteur ne répare pas les archives et ne télécharge pas les ressources manquantes. Utilisez les fonctions d’édition pour corriger, ou [exportez un rapport de diagnostic](troubleshooting.md#journaux-et-rapports-de-diagnostic) pour signaler un problème.

## Puissance des projecteurs pour la planification

La puissance du projecteur est sa consommation électrique maximale utilisée pour la planification. Une valeur absente apparaît comme **—** dans l’éditeur et la table ; un zéro déclaré reste zéro. La puissance Beam est informative et ne remplace jamais une consommation absente. La vue GDTF indique la source électrique et le total Beam informatif. Vérifiez Issues/Diagnostics pour les entrées ambiguës, déclarations contradictoires ou totaux Beam supérieurs à la consommation déclarée.

Modifier la puissance dans l’éditeur ou la table crée une correction de projet pour ce projecteur. Les calculs l’utilisent immédiatement et l’enregistrement PSTG la conserve sans modifier l’archive source. Les autres instances gardent leurs valeurs. Une correction explicite du document GDTF exige un champ électrique sûr existant ; Perastage conserve les informations de connecteur et enregistre une Revision standard. Sans champ sûr, gardez la correction dans le projet plutôt que d’inventer un connecteur ou une topologie d’alimentation. Les corrections de puissance du projet ne sont pas automatiquement écrites dans les GDTF à l’export MVR.

## Aperçus de l’éditeur de projecteurs

Dans le Fixture Editor, Preview commence par **All fixture**. Sélectionnez une Geometry pour examiner sa sous-arborescence en mode Solo ; les instances répétées de Model restent des choix distincts. La miniature GDTF située dessous prend en charge les PNG et SVG canoniques à la racine.

Dans Symbols, sélectionnez un Model **Standard GDTF** pour voir ses ressources exactes Top/Front/Side, ses dimensions et PrimitiveType. Les infobulles indiquent chemins, décalages d’insertion et diagnostics. Une ressource présente peut ne pas avoir d’aperçu pris en charge. Le groupe indépendant **Perastage project (PSTG)** affiche Top/Front/Side/Bottom pour tout le projecteur, avec une provenance distincte pour les remplacements utilisateur. Ouvrir ces aperçus et générer les symboles du projet préserve les ressources GDTF d’origine.
