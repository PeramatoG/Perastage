# Raccourcis et barre de commandes

Cette page regroupe les principales méthodes de travail au clavier, à la souris et avec la barre de commandes de l’aide intégrée, pour travailler plus rapidement sans quitter la documentation.

## Édition de scène

- `Ctrl+X` : couper les projecteurs, structures, palans et objets sélectionnés.
- `Ctrl+C` : copier les éléments de scène sélectionnés.
- `Ctrl+V` : coller les éléments copiés dans le projet actuel.

Ces commandes acceptent les sélections mixtes et sont disponibles dans le menu **Edit**. Pendant l’édition d’un champ texte ou d’une cellule, elles conservent leur comportement natif d’édition de texte.

## Raccourcis globaux

| Raccourci | Action |
| --- | --- |
| `Ctrl+N` | Nouveau projet |
| `Ctrl+L` | Charger un projet |
| `Ctrl+S` | Enregistrer le projet |
| `Ctrl+Q` | Fermer l’application |
| `Ctrl+Z / Ctrl+Y` | Annuler / rétablir |
| `Del` | Supprimer la sélection |
| `Ctrl+G` | Grouper la sélection active inter-tables |
| `Ctrl+U` | Retirer les objets sélectionnés de leur groupe direct |
| `F1` | Ouvrir l’aide |
| `Ctrl+Alt+D` / `Ctrl+Shift+D` | Répartir les projecteurs sur une structure / entre deux points |
| `P` / `R` | Activer la Console et préremplir `pos ` / `rot ` hors des champs modifiables |
| `F` | Activer la Console et préremplir `fixture ` hors des champs modifiables |
| `1 / 2 / 3 / 4` | Passer à Fixtures / Trusses / Hoists / Objects |

## Raccourcis des vues

| Raccourci | Action |
| --- | --- |
| `Z` | Ajuster la vue 2D/3D active |
| `NumPad1` | Vue de face |
| `NumPad3` | Vue de côté/droite |
| `NumPad7` | Vue de dessus |
| `NumPad5` | Réinitialiser l’orientation de la caméra 3D |
| `Arrow keys` (3D) | Orbiter la caméra |
| `Shift + Arrow keys` (3D) | Déplacer la caméra |
| `Alt + Arrow keys` (2D/3D) | Zoom avant/arrière |
| `Arrow keys` (2D) | Déplacer la vue |

## Raccourcis de mise en page

| Raccourci | Action |
| --- | --- |
| `Delete` | Supprimer l’élément de mise en page sélectionné |
| `Z` | Ajuster la vue de mise en page |

## Raccourcis de saisie Console

| Raccourci | Action |
| --- | --- |
| `Esc` | Quitter l’invite et réactiver les raccourcis de l’application |
| `Up / Down` | Parcourir l’historique des commandes |
| `Home` | Aller au début de la saisie après l’invite |
| `Left / Backspace` | Ne permet pas de passer avant l’invite |

## Raccourcis souris en vue 3D

| Action | Résultat |
| --- | --- |
| Glissement gauche | Orbiter la caméra |
| Maj + glissement gauche ou glissement central | Déplacer la caméra |
| Molette | Zoom avant/arrière |
| Clic gauche | Sélectionner le projecteur/la structure/l’objet sous le curseur |
| Maj/Ctrl + clic gauche | Basculer la sélection |
| Ctrl + glissement gauche | Sélection rectangulaire |
| Double-clic sur une étiquette de projecteur | Ouvrir le dialogue de patch |

## Procédure rapide de la barre de commandes

1. Activez la barre de commandes.
2. Hors des champs modifiables, utilisez `P`, `R` ou `F` pour activer et préremplir la Console.
3. Modifiez le texte de commande.
4. Exécutez et vérifiez le résultat dans les tables et vues.
5. Réutilisez l’historique pour les changements répétitifs.

## CLI expérimentale

Depuis 1.7.0, Perastage comprend une CLI expérimentale `perastage-cli` pour l’automatisation et les usages avancés. Exécutez-la dans un terminal :

| Plateforme | Exemple |
| --- | --- |
| Windows (PowerShell, installation par défaut) | `& "C:\Program Files\Perastage\perastage-cli.exe" --help` |
| macOS (application dans Applications) | `/Applications/Perastage.app/Contents/MacOS/perastage-cli --help` |
| Linux AppImage (remplacez le nom par votre téléchargement) | `./Perastage-1.7.0-x86_64.AppImage --cli --help` |
| Arch Linux | `perastage-cli --help` |

Utilisez `--version` pour vérifier la version installée. Windows et macOS n’ajoutent pas la CLI au PATH. Commandes et options restent en anglais. `inspect` lit les fichiers sans les modifier ; `scene` exige un fichier de sortie distinct et n’écrase jamais sa source ; `live` se connecte uniquement à une instance locale Perastage via loopback. Le caractère expérimental concerne la maturité du support et de la stabilité ; les règles de validation et d’atomicité s’appliquent toujours. L’adaptateur MCP se configure séparément et n’est pas inclus dans l’installateur. Consultez [Developer CLI](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/cli.md) pour la grammaire détaillée.

<!-- PERASTAGE_CONSOLE_HELP_BEGIN -->

## Commandes Console fréquentes et utiles

### Commandes de sélection

| Commande | Description |
| --- | --- |
| `clear` | Effacer les sélections de projecteurs, structures et objets |
| `f ...` | Sélectionner les projecteurs par ID |
| `t ...` | Sélectionner les structures par numéro d’unité, en effaçant d’abord leur sélection |

Exemples de syntaxe :

- ID unique : `f 12`
- Plage : `f 1-5`, `f 1 thru 5`, `f 1 t 5`
- Ajouter/retirer : `f + 10 - 3`
- Mixte : `f 1 3 5 7-9`

### Commandes de position et rotation

| Commande | Description |
| --- | --- |
| `pos x <values>` | Définir les positions X |
| `pos y <values>` | Définir les positions Y |
| `pos z <values>` | Définir les positions Z |
| `pos <x>,<y>,<z>` | Définir X/Y/Z ensemble |
| `x <values>` / `y <values>` / `z <values>` | Raccourcis d’axe pour `pos` |
| `rot x <values>` / `rot y <values>` / `rot z <values>` | Définir roulis/tangage/lacet |
| `rot x\|y\|z <values> --group` ou `--g` | Faire tourner la sélection comme un groupe |

Notes :

- Les positions Console et coordonnées de pivot utilisent les mètres ; les rotations, les degrés.
- Une valeur s’applique à tous les éléments sélectionnés.
- Deux valeurs répartissent linéairement la sélection du début à la fin ; `t` et `thru` sont des séparateurs facultatifs : `pos x -7 t 7`, `pos x -7 thru 7` et `pos x -7 7` sont équivalents.
- Utilisez `++` / `--` pour les décalages relatifs. Valeurs compactes et espacées sont acceptées, par exemple `pos x ++1.5`, `pos x ++ 1.5`, `pos z --1` et `pos z -- 1`.
- Le déplacement groupé suit les [préférences de sélection et déplacement](preferences.md#sélection-et-déplacement).
- Le pivot de rotation groupée est par défaut le centre de la boîte englobante de la sélection.
- Vous pouvez le remplacer par un triplet final `x,y,z`, par exemple `rot y ++45 --g -2.5,0,0`.

### Espaces de transformation

Les axes du monde sont la valeur par défaut. Les commandes relatives de position et rotation peuvent utiliser les axes locaux avec `--local` ou `-l` :

```text
f 1
pos x ++ 1
pos x ++ 1 --local
rot y ++ 45
rot y ++ 45 -l
rot z ++ 30 --group --local
```

Les modificateurs d’espace affectent les opérations incrémentales (`++` et `--`). Les positions absolues restent des coordonnées monde et les rotations absolues conservent le comportement Euler existant. Le bouton **Local Axes** est persistant et affecte uniquement les transformations dans la vue ; l’historique et les scripts restent déterministes et n’héritent pas de son état.

<!-- PERASTAGE_CONSOLE_HELP_END -->

## Focus et priorité

- Les raccourcis sont bloqués pendant la saisie dans un champ ou une cellule modifiable.
- Les raccourcis de vue s’exécutent dans la vue active.
- Les raccourcis propres à un dialogue restent limités à celui-ci.

## Raccourcis locaux du dialogue de création à partir de texte

| Raccourci | Action |
| --- | --- |
| `↑` / `↓` | Parcourir les suggestions |
| `Enter` ou `Tab` | Accepter la suggestion sélectionnée |
| `Esc` | Fermer les suggestions sans fermer le dialogue |

Pour l’édition des pages et la souris en mise en page, consultez [Mises en page et PDF](layouts.md).
