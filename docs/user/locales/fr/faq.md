# FAQ

## Perastage est-il un visualiseur DMX en temps réel ?

Non. Il se concentre sur l’édition de scène, la vérification MVR et la documentation technique. Consultez la [présentation des fonctions](features.md).

## Quelle est la différence entre PSTG et MVR ?

PSTG conserve le contexte Perastage, les mises en page et les symboles du projet pour poursuivre l’édition. MVR échange la scène avec d’autres applications. Consultez [Ouvrir des fichiers MVR](opening-mvr-files.md).

## Quels systèmes de bureau sont pris en charge ?

Windows, macOS et Linux. Consultez [Installation](installation.md) et les paquets de votre version.

## Perastage peut-il importer et exporter des MVR ?

Oui, en ouvrant un nouveau projet ou en fusionnant avec le projet actuel. Consultez [Ouvrir des fichiers MVR](opening-mvr-files.md).

## Qu’est-ce que l’Inspecteur MVR / GDTF ? Est-il en lecture seule ?

Il parcourt instantanés, archives externes, définitions, ressources et diagnostics sans modifier le projet ni le fichier. Consultez l’[Inspecteur](inspector.md).

## Perastage peut-il télécharger des GDTF ?

Oui, via **Tools → Download GDTF**, avec un compte GDTF Share et un accès réseau. Consultez [Téléchargement GDTF](gdtf-download.md).

## Peut-on consulter le catalogue GDTF en cache hors ligne ?

Oui, si un cache utilisable existe déjà. Le téléchargement exige toujours une authentification en ligne. Consultez la [navigation en cache](gdtf-download.md#navigation-en-cache-et-hors-ligne).

## Les mises en page et PDF sont-ils pris en charge ?

Oui. Composez des pages imprimables et exportez la mise en page sélectionnée avec **File → Print Layout...**. Consultez [Mises en page et PDF](layouts.md).

## Où sont les journaux ? Comment signaler utilement un problème ?

Utilisez **Help → Open Logs Folder** et **Help → Export Diagnostic Report...**. Indiquez les étapes de reproduction et examinez les fichiers avant partage. Consultez [Dépannage](troubleshooting.md#journaux-et-rapports-de-diagnostic).

## L’automatisation externe est-elle disponible ?

OSC et MCP sont des intégrations locales expérimentales nécessitant une configuration séparée. Consultez la [documentation d’intégration](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/index.md) et l’[adaptateur MCP](https://github.com/PeramatoG/Perastage/blob/main/docs/developer/mcp_adapter.md). Pour les commandes intégrées quotidiennes, consultez [Raccourcis et barre de commandes](shortcuts-and-command-bar.md).
