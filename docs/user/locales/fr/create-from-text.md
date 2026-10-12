# Créer à partir de texte

Utilisez **Tools → Create from text** pour transformer une fiche technique en scène initiale. Vérifiez le résultat avant de l’utiliser comme documentation de spectacle.

## Entrées et procédure

Le texte collé et les fichiers `.txt` et `.pdf` sont pris en charge ; le texte des PDF est extrait avant l’analyse.

1. Ouvrez **Tools → Create from text** et collez du texte ou chargez une fiche technique.
2. Choisissez **Apply filter** pour nettoyer les lignes candidates, puis vérifiez et modifiez le texte filtré.
3. Choisissez **Create**.
4. Si **Resolve fixture types** s’ouvre parce que des définitions doivent être résolues, vérifiez les propositions et choisissez **Resolve and create** pour confirmer le plan et lancer les téléchargements sélectionnés.

## Résolution des types de projecteurs

Les types connus utilisent le dictionnaire actif. Pour les types non résolus, vérifiez les suggestions GDTF Share en cache, choisissez **Use suggested** pour une correspondance fiable, ou **Search...** pour choisir un profil. Vous pouvez corriger un type dans la table et décocher **Create** pour ignorer ses projecteurs tout en conservant les autres éléments.

L’ouverture de la vérification ne nécessite pas de connexion. La première acquisition du catalogue ou un téléchargement peut l’exiger. Les correspondances acceptées sont mémorisées pour les prochaines fiches uniquement après confirmation. Annuler avant la création laisse la scène et les correspondances inchangées.

### Profil générique de secours

**Use generic** fournit un profil de secours pour cet import, sans enregistrer de correspondance fictive. Un catalogue absent, une connexion annulée ou un échec réseau permettent toujours une création générique. Les erreurs récupérables de téléchargement, validation, mode, authentification ou dictionnaire n’affectent que le type concerné ; le reste de l’import continue.

### Modes DMX

Choisissez explicitement le mode voulu lorsqu’un profil en propose plusieurs. Double-cliquez la cellule **Mode** pour examiner les choix. Les suggestions sans mode sélectionné utilisent un profil générique de secours. Les changements de mode confirmés du dictionnaire sont enregistrés après **Resolve and create**.

## Contenu créé et principes d’analyse

Selon le texte, la procédure crée des projecteurs, structures, palans/moteurs et objets de scène comme des écrans. Les entrées d’écran ou de contrôle vidéo ne sont pas traitées comme des projecteurs.

Les motifs utiles comprennent les quantités (`12 Spot`), lignes combinées (`8 Wash + 4 Beam`), positions comme `LX1`, `FLOOR` et `SIDES`, ainsi que les structures/tubes et palans. Les [préférences](preferences.md#import) Rider Import contrôlent le patch automatique, les calques et le placement LX par défaut.

## Saisie automatique et validation

L’éditeur suggère des mots-clés et des termes du dictionnaire local. Utilisez Haut/Bas pour naviguer, Entrée ou Tab pour accepter et Échap pour fermer les suggestions.

Après création, vérifiez les tables Fixtures, Trusses, Hoists et Objects, les modes DMX et le patch. Contrôlez le placement en [2D et 3D](views.md), enregistrez le PSTG et [exportez en MVR](opening-mvr-files.md#exporter-mvr) si nécessaire.

Les contrats détaillés d’analyse et de placement figurent dans les [règles texte-vers-scène](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/text_to_scene_rules.md).
