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
