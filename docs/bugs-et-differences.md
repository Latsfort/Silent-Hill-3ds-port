# Bugs et différences : retail PS1 / port 3DS

Relevé du 27 octobre 2026. Référence : décomp US dans `vendor/silent-hill-decomp`,
ressources originales et code actuel du port. Classement du plus au moins important.
Le boss reste laissé de côté pour les corrections.

**Périmètre :** différences actuellement identifiées, pas inventaire exhaustif certifié.
Audit du code et vérifications ciblées en émulateur Old 3DS ; aucun parcours complet
du jeu ni essai sur console effectué pour ce relevé.
« Confirmé » désigne une différence constatée dans le code, sans garantie sur son rendu réel.
Les anciennes notes de `PORTING.md` ne sont pas une preuve : plusieurs systèmes ont évolué.

## Bloquants

**D01 — Histoire complète non jouable — documenté, couverture non revérifiée intégralement.**
Le retail contient les zones et événements jusqu'aux fins. Le suivi déclare l'école
partielle et les zones suivantes/fins incomplètes. Une entrée dans `overlay_data.h`
prouve les paramètres d'une zone, pas que son parcours est intégré.
Sources : [suivi](porting-next.md), [paramètres](../source/game/overlay_data.h),
[cartes retail](../vendor/silent-hill-decomp/src/maps).

**D02 — Sauvegarde persistante et chargement — fonctionnels, présentation encore différente.**
Le carnet du café et les événements SaveMenu intégrés écrivent maintenant sur SD.
LOAD restaure les cartes prises en charge, la position, l'inventaire et les drapeaux.
Deux cartes de 165 emplacements, confirmation d'écrasement, contrôle d'intégrité et
repli sur une copie précédente sont intégrés. Les tests couvrent fermeture/réouverture,
fichiers tronqués et corrompus ; le carnet puis LOAD passent dans Azahar Old 3DS.
Le format du port et l'écran générique ne reproduisent pas le fichier et l'interface PS1.
Sources : [savegame.c](../source/game/savegame.c), [points avancés](debug-checkpoints.md),
[événement retail du café](../vendor/silent-hill-decomp/src/maps/map0_s01/map0_s01_events_data.c),
[sauvegarde retail](../vendor/silent-hill-decomp/src/screens/saveload/saveload.c).

## Écarts majeurs de gameplay

**D03 — Combat de la ruelle simplifié — confirmé, non corrigé.**
A retire directement 20 PV aux enfants proches. Leur déplacement suit une poursuite
simple ; les attaques et collisions de combat retail ne sont pas intégrées à ce chemin.
Sources : `grey_update`/`SH_ALLEY_FIGHT` dans [alley.c](../source/game/alley.c),
[contrôleur retail](../vendor/silent-hill-decomp/src/bodyprog/player_control.c).

**D04 — Sortie du cauchemar sans la mort retail — confirmé, non corrigé.**
Le port passe au café après élimination des ennemis ou 45 secondes. Le retail pose
le drapeau 25 à la fin de l'animation de mort de Harry dans MAP0_S00, puis suit
les événements 25 → 26 → chargement du café.
Sources : [alley.c](../source/game/alley.c), `PlayerState_Death` dans
[player_control.c retail](../vendor/silent-hill-decomp/src/bodyprog/player_control.c),
[événements retail](../vendor/silent-hill-decomp/src/maps/map0_s00/map0_s00_events_data.c).

**D05 — Comportement des ennemis de rue simplifié — confirmé.**
Le port emploie poursuite par distance et délais de morsure pour chiens et ennemis
volants, au lieu de leurs machines d'états complètes.
Sources : `dogs_update` dans [town.c](../source/game/town.c),
[Groaner retail](../vendor/silent-hill-decomp/src/maps/characters/groaner.c),
[Air Screamer retail](../vendor/silent-hill-decomp/src/maps/characters/air_screamer.c).

**D06 — IA de l'ennemi du café également simplifiée — confirmé.**
La section correspondante de `cafe.c` décrit explicitement cette simplification.
Sources : [cafe.c](../source/game/cafe.c),
[Air Screamer retail](../vendor/silent-hill-decomp/src/maps/characters/air_screamer.c).

**D07 — Impacts et dégâts en ville différents — confirmé.**
`try_hit_dogs` utilise distance et cône puis retire directement les PV. Ce chemin
ne consulte pas le décor pour bloquer les tirs. Les fonctions de rayons déjà
adaptées ailleurs ne rendent pas ce chemin automatiquement fidèle.
Sources : [town.c](../source/game/town.c),
[combat retail](../vendor/silent-hill-decomp/src/bodyprog/bodyprog_combat_8008A058.c),
[rayons du port](../source/game/combat_ray_world.c).

**D08 — Couverture des armes incomplète — confirmé dans les chemins café/ville examinés.**
Ces chemins reconnaissent pistolet et couteau. Posséder une autre arme ne prouve
pas que ses tirs, animations et dégâts y sont intégrés.
Sources : combat dans [town.c](../source/game/town.c) et [cafe.c](../source/game/cafe.c),
[combat retail](../vendor/silent-hill-decomp/src/bodyprog/bodyprog_combat_8008A058.c).

**D09 — Tir maintenu incohérent entre café et ville — corrigé.**
Le pistolet du café accepte maintenant A maintenu comme le chemin retail ; le
couteau exige un nouvel appui. Tests à 30/60 images/s : cadence, consommation de
munitions, relâchement et couteau. Les autres écarts de combat restent D06–D08.
Sources : [cafe.c](../source/game/cafe.c), [town.c](../source/game/town.c),
[contrôleur retail](../vendor/silent-hill-decomp/src/bodyprog/player_control.c).

**D10 — Déplacement libre piloté par des vitesses constantes — implémentation confirmée, écart à mesurer.**
Le port applique vitesses fixes de marche/course et rotation proportionnelle au
temps ; le retail calcule le mouvement selon ses états et animations. Les trajets
scriptés adaptés récemment ne remplacent pas le déplacement libre global.
Sources : `update_player` dans [game.c](../source/game/game.c),
[contrôleur retail](../vendor/silent-hill-decomp/src/bodyprog/player_control.c).

## Scènes et présentation

**D11 — Vidéo de la rue absente — corrigé, fidélité visuelle sur console à vérifier.**
FMV 16 utilise désormais les 119 images originales et les paquets XA extraits de
M2_01190, à 15 images/s, avec retour au jeu et saut par START. Lecture complète
jusqu'à l'image 118 et saut à l'image 63 vérifiés dans Azahar Old 3DS. Les tests
natifs comparent les images décodées, la cadence à 30/60 Hz et le parcours audio.
Les paquets audio de cette vidéo sont silencieux dans la ressource originale.
Sources : `SH_ALLEY_FMV` dans [alley.c](../source/game/alley.c),
[événements retail](../vendor/silent-hill-decomp/src/maps/map0_s00/map0_s00_events_data.c),
[lecteur retail](../vendor/silent-hill-decomp/src/screens/stream/stream.c).

**D12 — Mise en scène du corps dans la ruelle absente — confirmé ; ordre seul corrigé.**
Le retail déplace Harry puis montre le corps avec une caméra liée à sa tête.
Le port présente les répliques avec caméra scriptée désactivée et fondu noir.
Sources : `SH_ALLEY_NIGHTMARE` dans [alley.c](../source/game/alley.c),
`MapEvent_CutsceneAlleyNightmare` dans
[map0_s00_2.c retail](../vendor/silent-hill-decomp/src/maps/map0_s00/map0_s00_2.c).

**D13 — Scène de l'obscurité seulement partiellement reprise — confirmé.**
Le déclenchement et la validation des messages ont été corrigés. Le retail contient
encore des étapes de chargement de flamme, d'attente et d'animation absentes du
court enchaînement actuel. La scène entière ne peut donc pas être déclarée fidèle.
Sources : `SH_ALLEY_DARKER` dans [alley.c](../source/game/alley.c),
`MapEven_CutsceneAlleyGetsDarker` dans
[map0_s00_2.c retail](../vendor/silent-hill-decomp/src/maps/map0_s00/map0_s00_2.c).

**D14 — Trajets, redirections et certaines caméras de Cheryl réécrits — confirmé.**
Le port utilise des points écrits localement, une redirection commune et des
caméras calculées localement. Le champ de vue de repli est corrigé à 53,13 degrés,
le zoom de visée du café supprimé et le signe des orientations corrigé. L'alignement
horizontal vers Cheryl est testé ; trajets et scènes restent à porter intégralement.
La caméra de jeu de l'ouverture utilise désormais les zones MAP0_S00 originales,
avec chaque champ comparé au binaire retail vérifié par SHA1, y compris sa précision
Q4 et ses angles Q8. Le retour après caméra scriptée est testé à 30/60 images/s.
Les changements dynamiques de zones et les scènes réécrites restent à examiner.
Sources : `k_cheryl_*`, `in_redirect_zone`, `set_spot_cam`, `set_alley_cam_behind`
dans [alley.c](../source/game/alley.c),
[scènes retail](../vendor/silent-hill-decomp/src/maps/map0_s00/map0_s00_2.c).

**D15 — Zone d'entrée dans la ruelle élargie — corrigé.**
La zone ajoutée est supprimée. Les deux événements et leurs points sont générés
depuis le retail ; la comparaison utilise ses coordonnées Q12. Tests des limites
et approche avec collisions des IPD originaux : la zone est atteignable depuis la rue.
La mise en scène après déclenchement reste concernée par D14.
Sources : [alley.c](../source/game/alley.c),
[points retail](../vendor/silent-hill-decomp/src/maps/map0_s00/map_points.h).

**D16 — Animations des personnages de la ruelle parcourues génériquement — confirmé.**
`sh_alley_actor_keyframe` boucle à cadence fixe sur au plus vingt images pour Cheryl
et les enfants activés, sans sélectionner l'ensemble des animations selon leurs états retail.
La pose fixe des enfants avant activation est corrigée : image 477 extraite de
Stalker_Init, sans boucle pendant StalkerControl_1. Leurs positions et orientations
sont maintenant générées depuis les deux fonctions de création retail et comparées.
Ils ne modifient plus la caméra comme des ennemis actifs avant le cauchemar.
Sources : [alley.c](../source/game/alley.c),
[personnages retail](../vendor/silent-hill-decomp/src/maps/characters).

**D17 — Mort et fin de partie simplifiées — confirmé.**
Le café utilise encore délai/fondu ; le texte ajouté « Harry collapsed... » est supprimé.
En ville, les PV
épuisés peuvent déclencher directement la phase de fin de partie, sans la séquence
complète de mort retail.
Sources : [cafe.c](../source/game/cafe.c), [game.c](../source/game/game.c),
`PlayerState_Death` dans [player_control.c retail](../vendor/silent-hill-decomp/src/bodyprog/player_control.c).

## Interfaces et ajouts de développement

**D18 — Menu principal différent — confirmé, ajout du port.**
La version normale affiche START et LOAD avec les libellés retail. Les raccourcis
café/combat/rues et la visionneuse sont réservés au développement et aux tests.
Le menu titre, CONTINUE et les options ne sont pas encore reproduits intégralement.
Sources : [game.c](../source/game/game.c),
[titre retail](../vendor/silent-hill-decomp/src/bodyprog/events/title.c),
[options retail](../vendor/silent-hill-decomp/src/screens/options/options.c).

**D19 — Informations et textes ajoutés à l'écran — partiellement corrigé.**
Les PV numériques, coordonnées POS, titres de zone et aides de développement sont
masqués en jeu normal. Les aides de combat et de raccourcis du café sont supprimées.
L'inventaire, la fin de partie et les écrans génériques restent à comparer au retail.
Sources : construction de `info` dans [game.c](../source/game/game.c),
messages dans [cafe.c](../source/game/cafe.c).

**D20 — Capacité de personnages simultanés différente — limite corrigée à six.**
Les tableaux et réservations du rendu utilisent maintenant les six slots retail.
Le démarrage et les tests vidéo/sauvegarde passent dans Azahar Old 3DS après ce
changement. Une rencontre avec six personnages et la mémoire sur console restent à vérifier.
Sources : `SH_GFX_MAX_ACTORS` dans [gfx.h](../include/sh3ds/gfx.h),
`NPC_COUNT_MAX` dans [chara.h retail](../vendor/silent-hill-decomp/include/bodyprog/chara/chara.h).

**D21 — Raccourcis de développement accessibles — corrigé en version normale.**
START+L+R, START+SELECT et les changements de zone de développement sont désactivés
par défaut. Ils nécessitent DEVELOPMENT=1 ou un test automatisé actif.
Sources : [game.c](../source/game/game.c), [documentation](debug-checkpoints.md).

## Couverture documentée à détailler, hors boss

**D22 — École encore partielle — documenté ; inventaire exhaustif manquant.**
`sh_school_begin` filtre les événements gérés. Le suivi signale encore des
événements, objets, ventilateurs et effets d'eau non intégrés dans l'école
alternative, ainsi que l'événement UFO optionnel de l'étage supérieur.
Sources : [school.c](../source/game/school.c), [suivi](porting-next.md),
[école alternative](retail-alternative-school.md),
[étage supérieur](retail-alternative-school-upper.md).

## Adaptations de plateforme : différences, pas bugs établis

- **Commandes** : boutons PS1 remappés et Circle Pad, voir [controls.h](../include/sh3ds/controls.h).
- **Rendu/affichage** : Citro2D/Citro3D et écrans 3DS à la place du GPU PS1.
  Aucun défaut précis de couleurs, filtrage, transparence ou brouillard n'est
  déduit sans comparaison visuelle. Voir [gfx.c](../source/platform/gfx.c).
- **Chargements** : ressources romfs et reconstruction progressive des fenêtres
  de carte. Durées comparées non mesurées. Voir [game.c](../source/game/game.c).
- **Son** : pilote adapté et SPU logiciel ; aucune différence audible précise
  affirmée sans écoute comparative. Voir [source/sound](../source/sound).

## Corrections déjà appliquées dans ce chat

- Déclenchement de l'obscurité repris des chemins/seuils retail et passages de portes ajoutés.
- Déclenchement du cauchemar à son volume retail, avant le combat.
- Messages concernés de la ruelle conservés jusqu'à validation avec A.
- Aides inventées de la ruelle et raccourci Y vers le café supprimés.

Ces corrections ne clôturent pas D03, D04, D11, D12 ou D13.
Vérification antérieure : compilation, progression à 30/60 FPS et régression
des collisions. Aucune preuve de fidélité visuelle sur console.

## Vérifications restantes pour compléter cette liste

Comparer, depuis les mêmes états : scènes et saut des scènes, chaque événement,
objet et énigme, armes/difficultés, inventaire/options/sauvegardes, déplacements
et collisions, caméras, animations, éclairage/brouillard/transparences, sons,
musiques/vidéos, chargements et performances sur Old 3DS. Ce sont des domaines
à vérifier, pas des bugs supplémentaires affirmés sans preuve.

Le boss reste hors du travail de correction actuel. Sa présence dans le suivi
ne constitue pas une validation d'un combat jouable.
