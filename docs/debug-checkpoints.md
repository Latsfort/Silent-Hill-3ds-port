# Points d'avance pour le debogage

Sur console, extraire `dist/silent-hill-3ds-old3ds.zip` a la racine de la carte
SD puis lancer le jeu depuis le Homebrew Launcher. Le fichier `.3dsx` contient
toutes les ressources ; le script de preparation ne s'execute pas sur la 3DS.
En cas d'echec, le diagnostic est ecrit dans `sh3ds_load.log` a la racine de la SD.

Sur PC, lancer **Jouer.cmd** a la racine du projet. Ce lanceur ouvre une copie
stable du jeu dans Azahar : les compilations suivantes peuvent modifier le
fichier de travail sans modifier celui dont l'emulateur lit les ressources.
Fermer Azahar puis relancer Jouer.cmd pour utiliser une nouvelle compilation.

Les huit nouvelles entrees suivent les anciens raccourcis du menu principal.
Haut/Bas fait defiler la liste ; A ou START charge l'etape selectionnee.
En jeu, maintenir START et appuyer sur SELECT (ou l'inverse) revient au menu
principal, y compris pendant une scene ou un chargement. La partie en cours est
abandonnee ; choisir ensuite une etape restaure son parcours de reference.

Chaque entree cree un etat neuf du parcours de reference, puis charge la zone
par le meme chemin que les changements de zone du jeu : geometrie, collisions,
camera, ambiance sonore, environnement et retour au controle du joueur.
Ce n'est pas une sauvegarde de la partie en cours.

- **Trois cles** : cafe termine, carnet et note lus, cle de maison utilisee,
  indications de la carte copiees, trois cles ramassees ; serrures encore fermees.
- **Apres Eclipse** : les trois cles ont ete consommees, les serrures et la porte
  sont ouvertes, la scene de nuit est terminee et la lampe est allumee.
- **Ecole, arrivee** : meme progression de rue, enigmes de l'ecole intactes.
- **Acide en poche** : plan de l'ecole et produit chimique pris ; statue intacte.
- **Medaillon or** : acide verse, medaillon pris ; emplacement encore vide.
- **Piano** : medaillon or pose ; piano encore a resoudre.
- **Medaillon argent** : piano resolu, medaillon pris ; emplacement encore vide.
- **Chaudiere alimentee** : les deux medaillons sont poses ; interrupteur encore
  a actionner et chaudiere arretee.

Les medaillons et le produit chimique restent dans l'inventaire lorsque les
fonctions retail correspondantes ne les retirent pas. Les notes et cles retirees
par le retail ne sont pas conservees. Les drapeaux de ramassage empechent les
doublons. Les marques de carte et la possession des plans sont restaurees.

Ce parcours n'ajoute pas les objets facultatifs disperses dans les rues et ne
simule pas une duree de jeu, des blessures ou des munitions depensees. Il ne
valide pas les scenes qui suivent l'etape choisie. Un point avance n'implemente
pas une scene encore absente du port.

Les donnees sont generees par `tools/gen_debug_checkpoints.py` a partir des
fonctions retail et des points de carte de `vendor/silent-hill-decomp`.
`--check` compare les nombres, objets, drapeaux et coordonnees generes avec ces
sources. `tools/debug_checkpoints_regression.c` verifie les etats cumulatifs et
les rechargements dans l'ordre inverse. `tools/run_checkpoint_regressions.ps1`
verifie les huit entrees du menu sous Azahar configure en Old 3DS.
