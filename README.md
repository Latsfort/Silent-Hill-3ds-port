# Silent Hill 3DS

Port shell natif **Nintendo 3DS** (cible **Old 3DS**) basé sur le dépôt de décompilation PS1 :

[shdecompilations/silent-hill-decomp](https://github.com/shdecompilations/silent-hill-decomp)

Le décomp est en sous-module Git dans `vendor/silent-hill-decomp`.

## État actuel (important)

Ce dépôt produit un **`.3dsx` installable** : boucle frame, pad, Citro2D/Citro3D, arenas mémoire fixes.

Ce n’est **pas** encore le jeu Silent Hill complet.

Le README upstream le dit clairement : le décomp PS1 **n’est pas port-ready** (PsyQ, overlays MIPS, adresses hardcodées). On ne compile pas `bodyprog` / `maps` tels quels sur ARM11.

| Composant | Statut |
|-----------|--------|
| Homebrew `.3dsx` | OK (après build) |
| Plateforme 3DS (gfx/input/mémoire) | OK |
| Logique et histoire | Port partiel : ouverture, café, ville et école ; boss et suite encore en cours |
| Maps / assets | Non inclus (copie légale requise) |

## Prérequis

1. **devkitPro** avec paquets 3DS :
   - Windows : installer via [devkitPro installer](https://devkitpro.org/wiki/Getting_Started)
   - Dans l’environnement MSYS2 devkitPro :
     ```bash
     pacman -S 3ds-dev
     ```
2. Variables d’environnement : `DEVKITPRO`, `DEVKITARM`
3. 3DS homebrew (Homebrew Launcher) pour lancer le `.3dsx`
4. (Plus tard) dump légal du jeu pour peupler `romfs/game/`

## Préparer les assets (une fois)

Mets `Silent Hill (USA).bin` + `.cue` dans `dump/`, puis dans PowerShell :

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_assets.ps1
```

Le script extrait le disque avec `dumpsxiso` et `extract.py` (outils du décomp), puis copie les 217 textures TIM dans `romfs/game/tim`. `dump/` et `romfs/game/` ne sont jamais versionnés.

## Build

Depuis **MSYS2 / devkitPro shell** (pas PowerShell brut, sauf si PATH configuré) :

```bash
cd /c/Users/nathan/Documents/silent-hill-3ds
make clean
make
```

Sortie attendue : `silent-hill-3ds.3dsx` (+ `.elf`, `.smdh`).

### Installer sur la 3DS

1. Copier `silent-hill-3ds.3dsx` sur la SD, ex. `sd:/3ds/silent-hill-3ds/`
2. Lancer via Homebrew Launcher
3. Au lancement : écran titre → **A / START = Nouvelle partie** (spawn map0 / THR comme le jeu PS1). **Y** = visionneuse debug. **SELECT** quitte depuis le titre.

Contrôles principaux : stick = déplacement, **A = action**, **R = visée**, **L = course**, **Y = carte**, **SELECT = inventaire**, **B = retour**. Voir aussi [les commandes](include/sh3ds/controls.h).

L’état détaillé et les limites des vérifications sont consignés dans [le suivi du port](docs/porting-next.md). La compilation et les tests sur ordinateur ne remplacent pas une validation sur une vraie Old 3DS.

## Structure

```
silent-hill-3ds/
  include/sh3ds/     API plateforme + jeu
  source/
    main.c           Entrée 3DS
    platform/        mémoire, gfx Citro, input
    game/            shell phases (boot/title/runtime)
  romfs/game/        assets privés (non versionnés)
  vendor/
    silent-hill-decomp/   référence PS1 (submodule)
```

## Feuille de route

1. Stabiliser ce shell sur Old 3DS (OK)
2. Charger un asset minimal depuis `romfs`
3. Porter des unités **pures** du décomp (maths, flags) sans PsyQ
4. Réécrire le rendu GPU PS1 → Citro3D
5. Brancher overlays maps un par un

## Légalité / licence

- Code de port shell : usage homebrew personnel
- Code décomp upstream : GPL-3 (voir `vendor/silent-hill-decomp/LICENSE`)
- **Assets / ISO Silent Hill** : copyright Konami — fournis uniquement depuis **ta** copie ; ne jamais committer ni redistribuer

## Crédits

- Décompilation : [shdecompilations/silent-hill-decomp](https://github.com/shdecompilations/silent-hill-decomp)
- SDK : [devkitPro](https://devkitpro.org/) (libctru, Citro3D, Citro2D)
