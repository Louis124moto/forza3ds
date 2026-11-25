# Forza3DS — TODO

> Cahier des charges consolidé le 25 novembre 2026. Toutes les cases sont des tâches à réaliser, pas des fonctionnalités déjà présentes.

## Objectif
Jeu de conduite 3D natif pour New Nintendo 3DS, écrit en C avec devkitARM, libctru et Citro3D. Priorité à une démo de conduite très soignée avant le monde complet.

## Contraintes
- [ ] Cible : New Nintendo 3DS / New Nintendo 3DS XL (ARM11).
- [ ] Exécutable natif 3DSX.
- [ ] Objectif : 30 FPS stables, à mesurer sur console réelle.
- [ ] Pas de 3D stéréoscopique dans la première version.
- [ ] Tester sur vraie New 3DS à chaque étape significative.
- [ ] Ne déclarer aucune fonctionnalité terminée sans code et vérification.
- [ ] Gérer proprement les fichiers externes invalides et les erreurs mémoire.

## Vision du jeu
- [ ] Jeu solo de conduite et d'exploration libre, avec courses organisées et sans circulation routière.
- [ ] Mélange d'exploration automobile, de conduite simulation et de courses.
- [ ] Grande île fictive d'inspiration japonaise, montagneuse et découpée en plusieurs péninsules.
- [ ] Biomes : montagnes, forêts, bambouseraies, campagnes, villages, villes, zones industrielles, côtes, plages, lacs et rivières.
- [ ] Ambiance contemplative et colorée, soleil uniquement et cycle jour/nuit rapide.
- [ ] Disciplines : route, drift, montagne, rallye/off-road et contre-la-montre.
- [ ] Garage 3D, collection de voitures, personnalisations visuelles et mécaniques.
- [ ] Véhicules abandonnés placés selon les biomes et les bâtiments, restaurés automatiquement à la découverte.
- [ ] Argent gagné par les courses ; pas de niveaux de personnage.

## Première démo jouable — priorité absolue
- [ ] Initialiser l'application native et les écrans.
- [ ] Initialiser Citro3D et afficher une scène 3D.
- [ ] Lire les commandes de la New 3DS, permettre leur reconfiguration, et exposer sensibilité de direction et zone morte du Circle Pad réglables. Pas d'option d'inversion gauche/droite.
- [ ] Afficher une voiture prototype, une route et un environnement minimal.
- [ ] Implémenter le système de caméra libre : orbite autour du véhicule au C-stick de la New 3DS, distance et hauteur réglables, recentrage automatique derrière le véhicule après relâchement et au redémarrage depuis l'arrêt, vitesse de recentrage réglable, vues extérieure et cockpit, positions personnalisées sauvegardables globalement (pas par véhicule). La caméra ne permet pas de déplacement vertical libre. La vue cockpit affiche le volant et le tableau de bord, avec position standard non réglable.
- [ ] Implémenter accélération, freinage, direction et mouvement.
- [ ] Ajouter collisions simples avec route et obstacles.
- [ ] Afficher vitesse et rapport sur l'écran inférieur.
- [ ] Mesurer les performances sur New 3DS et profiler avant d'ajouter des détails.
- [ ] Obtenir une démo stable et soignée avant d'élargir le périmètre.

## Architecture du moteur
- [ ] Moteur spécifique au jeu, écrit en C.
- [ ] Modules séparés : core, input, render, physics, vehicle, world, generation, assets, UI, audio et save.
- [ ] Citro3D pour le rendu PICA200 ; libctru pour les services système.
- [ ] Séparer strictement géométrie visuelle, définition physique et état runtime des véhicules.
- [ ] Boucle de jeu avec pas de simulation fixe configurable par profils prédéfinis (compromis performance / réalisme) et limite de rattrapage.
- [ ] Mesurer CPU, GPU, mémoire, temps de génération et temps de frame sur la console.
- [ ] Définir les budgets de mémoire, triangles, matériaux, textures et objets visibles à partir de mesures réelles.

## Physique automobile personnalisée
- [ ] Définir unités, échelle, repère et conventions de coordonnées.
- [ ] Implémenter masse, inertie, centre de gravité et forces longitudinales/latérales.
- [ ] Implémenter direction progressive avec inertie.
- [ ] Implémenter accélération, freinage et frein à main.
- [ ] Implémenter suspensions et transferts de charge.
- [ ] Implémenter adhérence dépendant de la vitesse et de la surface.
- [ ] Implémenter glisse, survirage, sous-virage et récupération.
- [ ] Implémenter moteur, rapports et transmissions automatique/manuelle.
- [ ] Ajouter progressivement ABS, antipatinage et aides configurables.
- [ ] Créer des comportements distincts par véhicule.
- [ ] Écrire des tests numériques déterministes pour les fonctions physiques critiques.

## Modèles 3D générés et personnalisables
### Modèles intégrés
- [ ] Générer en C des maillages simples pour voitures, roues, routes et objets de test.
- [ ] Utiliser ces modèles intégrés pour les prototypes et comme solution de repli.

### Importateur OBJ natif depuis la carte SD
- [ ] Scanner sdmc:/3ds/forza3ds/models/.
- [ ] Implémenter un parseur OBJ en C : sommets, UV, normales, groupes et faces.
- [ ] Gérer indices positifs/négatifs, faces triangulaires et polygonales.
- [ ] Trianguler les polygones et rejeter les géométries invalides.
- [ ] Prendre en charge un sous-ensemble documenté des matériaux MTL.
- [ ] Valider tailles, nombres d'éléments, indices, chemins et valeurs numériques.
- [ ] Définir des limites pour éviter les allocations excessives.
- [ ] Convertir les données en buffers compatibles Citro3D.
- [ ] Générer un cache binaire versionné dans sdmc:/3ds/forza3ds/cache/models/.
- [ ] Invalider le cache lorsque la source ou la version du convertisseur change.
- [ ] Afficher des erreurs compréhensibles si un modèle est absent ou incompatible.

### Paquet de modèle personnalisé
Exemple d'organisation :
sdmc:/3ds/forza3ds/models/cars/model-id/
  model.obj
  model.mtl
  textures/
  vehicle.cfg
- [ ] Définir vehicle.cfg : identifiant, nom, échelle, orientation, origine, points d'attache des roues et boîte de collision.
- [ ] Séparer carrosserie, roues et éléments optionnels.
- [ ] Ne pas déduire masse ou caractéristiques mécaniques depuis le maillage OBJ.
- [ ] Créer un menu listant les modèles détectés.
- [ ] Créer un aperçu 3D rotatif avant validation.
- [ ] Permettre importation, sélection, assignation et remplacement d'un modèle sur un véhicule.
- [ ] Conserver les réglages mécaniques lors du remplacement visuel.
- [ ] Enregistrer les assignations dans la sauvegarde.
- [ ] Fournir un paquet OBJ de démonstration libre de droits.

## Monde procédural
### Génération déterministe
- [ ] Générateur piloté par une graine personnalisable.
- [ ] Sous-graines distinctes pour relief, biomes, routes, villes, bâtiments et détails.
- [ ] Même graine = même relief, routes et villes.
- [ ] Autoriser des variations de détails sans modifier la structure principale.
- [ ] Versionner l'algorithme pour préserver les anciennes sauvegardes.

### Île, relief et biomes
- [ ] Générer une grande île montagneuse avec plusieurs péninsules.
- [ ] Générer littoral, baies, vallées, crêtes, cols, plaines et reliefs secondaires.
- [ ] Déterminer les biomes selon altitude, climat fictif et distance à la mer.
- [ ] Générer forêts, bambouseraies, végétation côtière, campagnes et zones urbaines.
- [ ] Générer rivières, lacs et cascades uniquement lorsque la topologie le permet.
- [ ] Ajouter des règles de cohérence géographique.

### Routes, villes et lieux
- [ ] Générer les routes en suivant le relief naturel.
- [ ] Réseau dense autour des villes et plus léger dans les zones sauvages.
- [ ] Générer routes principales, secondaires, chemins et passages cachés.
- [ ] Classer les segments selon leur usage : route, drift, montagne ou rallye.
- [ ] Valider pentes, rayons de virage, raccordements et continuité.
- [ ] Générer villes, villages, zones industrielles et bâtiments avec règles de densité et de taille.
- [ ] Placer les voitures abandonnées selon le biome et le type de bâtiment.
- [ ] En cas de zone invalide, relancer localement avec une sous-graine contrôlée.
- [ ] Une relance locale ne doit pas modifier les secteurs déjà validés.

### Monde logique et chargement 3D
- [ ] Représenter toute l'île dans une structure logique compacte.
- [ ] Charger en 3D uniquement les secteurs proches du joueur.
- [ ] Décharger les secteurs éloignés sans perdre leur identité procédurale.
- [ ] Définir niveaux de détail et distance d'affichage.
- [ ] Mesurer la mémoire utilisée par secteur et catégorie d'objets.

## Courses et IA
- [ ] Générer des parcours sur les routes disponibles.
- [ ] Vérifier continuité, sens, points de contrôle et faisabilité.
- [ ] Rejeter puis régénérer localement un parcours impossible si la correction échoue.
- [ ] Placer les événements sur des lieux physiques du monde.
- [ ] Implémenter progressivement route, drift, montagne, rallye et contre-la-montre.
- [ ] Ajouter une IA capable de suivre un parcours et de gérer freinage/accélération.
- [ ] Adapter le nombre d'adversaires au budget CPU.
- [ ] Calculer les récompenses selon classement et performance.
- [ ] Enregistrer les records personnels.

## Garage et véhicules
- [ ] Créer un garage 3D avec caméra d'inspection.
- [ ] Gérer la collection de véhicules.
- [ ] Séparer données visuelles, physiques et mécaniques.
- [ ] Permettre peintures, roues et modifications visuelles.
- [ ] Prévoir améliorations moteur, transmission, pneus et suspension.
- [ ] Permettre le réglage et la préparation détaillée des moteurs : régime maximal, pression de suralimentation du turbo et autres paramètres, avec des effets cohérents sur les performances et le son.
- [ ] Créer un véritable atelier accessible en jeu, avec une interface permettant de régler individuellement les paramètres de préparation moteur.
- [ ] Permettre de remplacer et configurer individuellement les pièces moteur (turbo, admission, échappement, injecteurs, pistons, arbre à cames, etc.), avec des effets cohérents sur les performances et la sonorité.
- [ ] Ne pas imposer de plafond artificiel aux réglages de préparation (pression de turbo, régime maximal et autres paramètres) ; faire ressentir clairement chaque modification dans le comportement du véhicule et la rendre nettement audible dans le son du moteur.
- [ ] Permettre le réglage détaillé de la boîte de vitesses dans l'atelier : nombre de rapports, rapports individuels et démultiplication finale, avec des effets réels sur l'accélération, la vitesse et le comportement du véhicule.
- [ ] Permettre le réglage des différentiels avant, central et arrière dans l'atelier, notamment leur taux de blocage et la répartition du couple, avec des effets réels sur la motricité et le comportement du véhicule.
- [ ] Ajouter découverte et restauration automatique des véhicules abandonnés.
- [ ] Gérer réparations et pièces récupérées/achetées.
- [ ] Ne pas implémenter d'usure générale permanente.

## Interface, audio et sauvegarde
- [ ] Écran supérieur : conduite ; écran inférieur : vitesse, compte-tours, rapport et informations.
- [ ] Carte consultable à tout moment.
- [ ] Caméra extérieure libre et caméra cockpit interchangeables, avec positions personnalisées globales sauvegardées ; voir les règles détaillées dans la section Première démo jouable.
- [ ] Audio moteur prioritaire, vent et ambiance environnementale.
- [ ] Adapter dynamiquement les ambiances sonores naturelles (vent, forêt, mer, oiseaux, etc.) au biome et à l'environnement traversés.
- [ ] Faire varier dynamiquement le volume des sons environnementaux, notamment le vent, en fonction de la vitesse du véhicule.
- [ ] Faire évoluer le son du moteur de manière réaliste selon le régime moteur (RPM) et la charge.
- [ ] Donner à chaque moteur une signature sonore propre selon ses caractéristiques (cylindrée, nombre de cylindres, aspiration et autres paramètres).
- [ ] Adapter le son du moteur à la vue cockpit avec une atténuation et un filtrage réalistes depuis l'habitacle.
- [ ] Adapter les sons à la pluie et aux surfaces mouillées : roulement, projections d'eau et bruits d'adhérence des pneus.
- [ ] Produire des sons de roulement et d'adhérence distincts et réalistes selon le matériau de surface (asphalte, gravier, terre, herbe).
- [ ] Spatialiser les sons en 3D selon leur direction et leur distance dans le monde.
- [ ] Adapter la réverbération sonore à l'environnement (tunnels, garages et espaces ouverts).
- [ ] Proposer des réglages de volume indépendants pour le moteur et l'ambiance environnementale.
- [ ] Cycle jour/nuit rapide avec éclairage adapté.
- [ ] Sauvegarder manuellement la progression (graine, version du générateur, position, garage, assignations, réglages, argent, records et découvertes), avec sauvegarde rapide accessible depuis le menu pause ; une seule sauvegarde de progression. Ne pas sauvegarder automatiquement la progression à la fermeture.
- [ ] Sauvegarder automatiquement les réglages de commandes, caméra et graphismes.
- [ ] Versionner les sauvegardes et gérer les fichiers corrompus sans écraser une sauvegarde valide.
- [ ] Menu de réglages graphiques : qualité des textures, ombres et distance d'affichage.
- [ ] Limiteur FPS configurable et compteur FPS temps réel activable/masquable ; aucun panneau de statistiques CPU/GPU/mémoire, benchmark intégré ou détection automatique des réglages.
- [ ] Mettre en pause la simulation physique lorsque le joueur met le jeu en pause.
- [ ] Couper automatiquement le son du jeu pendant la pause et restaurer le volume précédent à la reprise.
- [ ] Mettre automatiquement le jeu en pause lorsque le clapet de la New 3DS est fermé ; à la réouverture, rester en pause jusqu'à une action explicite du joueur pour reprendre.
- [ ] Régénérer les données immuables depuis la graine ; stocker explicitement tout état non dérivable.

## Qualité, tests et livraison
- [ ] Tests unitaires pour parseur OBJ, générateur et physique.
- [ ] Vérifications de limites mémoire et de données invalides.
- [ ] Compilation automatique devkitARM/libctru/Citro3D via GitHub Actions.
- [ ] Publier le 3DSX comme artefact CI lorsque le build réussit.
- [ ] Tester lancement, fermeture, contrôles et performances sur vraie New 3DS.
- [ ] Documenter les fonctionnalités réellement terminées.
- [ ] Ne pas confondre compilation réussie et fonctionnalité jouable.

## Ordre de réalisation
1. Base native et compilation.
2. Scène 3D, contrôles et caméra.
3. Première voiture et physique.
4. Route, collisions et HUD.
5. Profilage et optimisation sur New 3DS.
6. Importateur OBJ et assignation des modèles.
7. Générateur de terrain et streaming.
8. Routes, biomes et implantations.
9. Courses, IA et garage.
10. Sauvegarde, audio et finition.

## Décisions reportées
- Budgets mémoire et polygones établis par mesures réelles.
- Syntaxe définitive de vehicle.cfg et du cache figée avec l'importateur.
- Algorithmes détaillés du terrain et des routes choisis lors de leur implémentation.
- Caractéristiques physiques des voitures affinées avec les prototypes.