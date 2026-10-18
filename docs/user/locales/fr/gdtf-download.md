# Téléchargement GDTF

Utilisez **Tools → Download GDTF** pour rechercher et télécharger des profils de projecteurs depuis GDTF Share. Le chargement du catalogue en ligne et les téléchargements nécessitent un compte GDTF Share et une connexion Internet.

## Trouver un profil

1. Ouvrez **Tools → Download GDTF**. Un catalogue en cache utilisable s’ouvre immédiatement ; sinon, connectez-vous pour charger le catalogue. Annuler la connexion initiale annule l’opération.
2. Recherchez par fabricant, projecteur, UUID, révision ou nom de mode DMX. Tous les mots doivent correspondre, dans n’importe quel ordre ; les filtres Manufacturer et Fixture réduisent les résultats.
3. Sélectionnez une révision et vérifiez son créateur, ses dates, sa taille, ses identifiants et ses modes DMX avec leur nombre de canaux avant le téléchargement.
4. Téléchargez le profil sélectionné et vérifiez la définition et le mode du projecteur dans votre projet ou dictionnaire.

## Navigation en cache et hors ligne

Un catalogue récent est réutilisé pendant une heure. Un cache ancien est actualisé en arrière-plan, sans interrompre la consultation ; un échec d’actualisation conserve les résultats en cache. La consultation du cache fonctionne hors ligne sans connexion au compte. Les téléchargements nécessitent une authentification en ligne et attendent la fin d’une actualisation active.

Un téléchargement échoué ou annulé conserve le profil de destination existant.

## Identifiants et profils locaux

Les mots de passe utilisent le stockage sécurisé du système lorsqu’il est disponible. Sinon, Perastage vous avertit et peut utiliser les identifiants pour l’opération en cours sans conserver le mot de passe. Vous devrez peut-être le saisir lors d’une autre session. Le nom d’utilisateur peut être mémorisé comme aide à la connexion.

Utilisez **Tools → Edit dictionaries** pour gérer les correspondances de profils et **Tools → Open user library folder** pour trouver les contenus locaux. Consultez le [dépannage des identifiants](troubleshooting.md#identifiants-gdtf-share) si les mots de passe ne sont pas conservés.

## Résoudre les profils lors d’un import MVR

Le dialogue MVR/App/Download reste inchangé. Download ouvre la file avec des
suggestions sûres et les profils MVR ou de l’application disponibles. Sélectionnez
une ligne pour examiner les révisions et avertissements, même après un succès.
**OK** reste actif lorsque la file est inactive : il traite les sélections en
attente puis ferme la fenêtre. Il est désactivé pendant l’analyse et les
téléchargements. Les nouveaux échecs ou choix non résolus gardent la fenêtre ouverte
pour vérification. Il n’y a plus de bouton Appliquer séparé. Les choix acceptés
s’appliquent à toutes les instances de ce type.

Choisir **Download** demande le fichier du dépôt lorsque le modèle correspond sans
ambiguïté, même si le nom du mode diffère. Par exemple, `DMX Mode` n’empêche pas de
télécharger ELP PAR avec les modes `Basic` et `Compact`. Le mode DMX est attribué automatiquement après téléchargement, sans sélecteur ni
confirmation de compatibilité. Priorité : le nom indiqué dans le MVR (sans tenir
compte de la casse, des espaces et de la ponctuation), un mode unique ayant le nombre
de canaux du profil d’origine, puis, si ce nombre manque, un mode unique déduit de
l’espacement répété et égal du patch. Les détails indiquent la preuve utilisée.
Choisir un autre candidat ou rechercher dans le catalogue complet utilise la même
attribution automatique ; les modes absents du catalogue sont lus dans le paquet téléchargé.

Si le mode reste ambigu, le fichier reste téléchargé et le profil et les modes MVR
d’origine restent sélectionnés. La ligne explique ce repli ; **OK** permet de continuer
sans choisir de mode. **Choisir le mode DMX...** reste un réglage manuel facultatif.
Plusieurs modes importés sont conservés plutôt que remplacés par un seul.

Chaque ligne possède les cases exclusives **MVR**, **Perastage** et **Fichier sélectionné**.
Les sources indisponibles sont désactivées. MVR conserve les modes importés ;
Perastage utilise un profil disponible de la bibliothèque ; Fichier sélectionné
rétablit le dernier choix téléchargé ou local. Revenir au téléchargement réutilise
le paquet validé. **Téléchargé ; identique au MVR d’origine** signifie que les octets
des deux fichiers ont été comparés et correspondent exactement ; un nom ou un
FixtureTypeID commun ne prouve pas l’identité des fichiers.

**Rechercher dans GDTF Share...** utilise le navigateur existant.
**Charger un GDTF local...** valide le fichier et copie le profil dans les
ressources de l’import sans modifier l’original. Les profils locaux compatibles
reçoivent aussi un mode automatiquement si les données importées le permettent.
Sinon, un choix manuel est proposé. Vérifiez les différences de modèle, de mode et d’empreinte. Les sources de secours
utilisables peuvent être sélectionnées explicitement sur la même ligne.

La fermeture automatique se règle uniquement dans **Préférences → MVR Import/Export**
et reste désactivée par défaut. Activée, elle poursuit l’import avec les choix sûrs
terminés. Les ambiguïtés et échecs restent ouverts pour vérification.

La file affiche l’activité du catalogue et de la connexion, le temps écoulé,
l’avancement par type de projecteur et les octets téléchargés. Le vert indique un
téléchargement, une réutilisation ou une sélection locale réussi ; l’ambre indique
le profil MVR d’origine ou le secours de l’application ; le rouge indique un échec
ou un profil non résolu.

Les candidats affichent tous leurs modes et nombres de canaux disponibles. Les
métadonnées absentes sont indiquées comme indisponibles, jamais comme zéro canal,
puis actualisées depuis le paquet téléchargé. Le mode importé et son empreinte
connue figurent dans les détails. La déduction du patch exige au moins deux intervalles adjacents égaux dans le même
univers, sans adresses dupliquées ou invalides ni projecteur intermédiaire d’un autre
type. Elle ne remplace jamais un nombre de canaux d’origine connu et ne départage
pas les modes de même empreinte. Des adresses libres peuvent être incluses ; une
empreinte identique ne garantit pas les mêmes fonctions DMX.

**Annuler**, **Échap** et fermer la fenêtre arrêtent la file et poursuivent l’import
avec les profils et modes MVR d’origine pour toutes ses lignes. Toutes les
substitutions de la file sont abandonnées, y compris les téléchargements terminés
et les sélections locales. **OK** accepte les résultats.

**OK** relance aussi les téléchargements échoués. Pour continuer avec un profil
d’origine après un échec, sélectionnez sa source **MVR** puis **OK**. Annuler
l’authentification arrête les tentatives restantes sans répéter le dialogue de connexion.
