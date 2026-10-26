# Notes de portage

## Pourquoi on ne compile pas le décomp tel quel

`vendor/silent-hill-decomp` cible le **PlayStation 1** :

- toolchain MIPS / PsyQ (`libgpu`, `libgte`, `libcd`, …)
- overlays (`BODYPROG.BIN`, maps) chargés à des adresses fixes
- rendu ordered-table + VRAM 1 Mo PS1

La 3DS est **ARM11 + PICA200**. Il faut une couche plateforme séparée (ce dépôt) et porter le code **par modules**.

## Correspondance

| Décomp PS1 | Rôle | Sur 3DS |
|------------|------|---------|
| `src/main` | bootstrap / mémoire | `source/main.c` + `platform/memory.c` |
| `src/bodyprog` | logique jeu | à porter dans `source/game/` |
| `src/screens` | écrans / UI | Citro2D d’abord |
| `src/maps` | stages | streaming `romfs` + Citro3D |
| `include/psyq` | SDK Sony | **ne pas linker** — remplacer |

## Règles mémoire (Old 3DS)

- Pas de `malloc` dans la boucle frame
- Arenas : `sh_memory_frame_arena()` (reset chaque frame), `sh_memory_level_arena()` (reset au changement de map)
- Budget : 1 MiB frame (`SH3DS_FRAME_ARENA_BYTES`) + 5 MiB streaming de la map (`SH3DS_STREAM_ARENA_BYTES`) + 10 MiB niveau (`SH3DS_LEVEL_ARENA_BYTES`), pools statiques
- Mémoire GPU (linéaire) réservée une fois au démarrage : deux vertex buffers de map (affiché / en construction), jusqu'à 6 pages d'atlas de textures, texture de la visionneuse
- Le changement de morceau est étalé sur plusieurs frames (~5 ms par frame) pendant que l'ancienne vue reste affichée
- Collisions marche (`source/game/coll.c`) : sol/pentes (surfaces), murs de bâtiments (arêtes one-sided des subcells), props/panneaux (cylindres `ptr_18`). Noclip optionnel (L+R).
- Harry : `HERO.ILM` + `HB_BASE.ANM` + atlas GPU `HERO.TIM` (multi-CLUT), idle / marche / course (R), caméra chase, collisions avec montée de marches.
- Marche = anim walk ; **R** = course. Escaliers : lèvres one-sided traitées via surfaces au-dessus des pieds.
- Savegame / flags / inventaire : `source/game/savegame.c` + maths Q12 `include/sh3ds/fixed.h` (API bodyprog-like, pas le layout PS1 binaire).
- Radio stub : `source/game/radio.c` (intensité HUD ; pas encore d’audio SPU).

- Boot : titre → Nouvelle partie → spawn `map0_s00` / THR à `MAP_POINTS[0]` (-6.2, 160.5). Cutscenes pas encore portées.

## Prochaine unité utile à porter

1. Cutscene / events map0 (ouverture allée Cheryl) — ou skip flag + spawn post-allée
2. Spawner / dessin d’un ennemi (AIR_SCRE / STLKER)
3. Audio réel (radio static + SFX)
