# VS Code — Configuration du workspace STM32N6570-DK

Ce répertoire contient la configuration VS Code pour le développement, le build,
le flash et le debug du projet STM32N657 sur la carte STM32N6570-DK.

> **Note** : les chemins d'outils référencent CubeIDE 2.1.1 installé dans
> `C:\ST\STM32CubeIDE_1.13.0\`. Adapter les chemins si votre installation diffère.

---

## Extensions requises

| Extension | ID Marketplace | Rôle |
|-----------|---------------|------|
| **Cortex-Debug** | `marus25.cortex-debug` | Debugger ARM Cortex-M via GDB/ST-LINK |
| **C/C++** | `ms-vscode.cpptools` | IntelliSense, navigation de code, debug source |
| **Native Debug** | `webfreak.debug` | Support GDB générique (optionnel) |

### Installation rapide

```
code --install-extension marus25.cortex-debug
code --install-extension ms-vscode.cpptools
```

---

## Toolchain — CubeIDE 2.1.1 (Build 28236, mars 2026)

Tous les outils proviennent de CubeIDE 2.1.1, sous-répertoire `plugins/` :

| Outil | Version | Plugin |
|-------|---------|--------|
| GCC ARM | 14.3.1 | `gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740` |
| Make | 2.2 | `make.win32_2.2.100.202601091506` |
| GDB Server | 7.13.0 | `stlink-gdb-server.win32_2.2.400.202601091506` |
| CubeProgrammer | 2.22.0 | `cubeprogrammer.win32_2.2.400.202601091506` |

---

## Fichiers de configuration

### `tasks.json` — Build & Flash

| Tâche | Description | Raccourci |
|-------|-------------|-----------|
| **FSBL: Build** | Compile FSBL avec GCC 14.3 (make -j4) | Ctrl+Shift+B |
| **FSBL: Load to RAM + Run** | Flash ELF en SRAM + set MSP/PC + démarrage | Barre d'état |
| **FSBL: Load to RAM (halted)** | Flash ELF en SRAM, CPU reste halté (pour debug) | — |
| **FSBL: Build + Load + Run** | Build puis flash puis run | Barre d'état |
| **FSBL: Build + Load (debug)** | Build puis flash halté (preLaunchTask du debug) | — |
| **STM32: Reset Board** | Hardware reset de la carte | Barre d'état |
| **STM32: Erase All** | Effacement complet de la flash | Barre d'état |

#### Séquence de flash (3 étapes STM32_Programmer_CLI)

```
1. mode=UR reset=HWrst -d <elf>     → download ELF en SRAM (0x34180000)
2. mode=HotPlug -coreReg MSP=0x34200000 PC=<entry>  → initialise registres
3. mode=HotPlug -s                    → démarre l'exécution
```

**Point critique** : le numéro de série ST-LINK (`sn=004200253234511233353533`) est
codé en dur. Remplacer par le vôtre (visible avec `STM32_Programmer_CLI -l`).

### `launch.json` — Debug GDB

| Configuration | Description |
|---------------|-------------|
| **FSBL: Debug (ST-LINK)** | Build + flash + GDB attach + break à `main()`. Appuyer sur F5. |
| **FSBL: Attach (ST-LINK)** | Se connecte au programme déjà en cours (sans flash). |

Paramètres clés du GDB server :
- `-m 1` : Access Port 1 (Cortex-M55 Secure, obligatoire avec TrustZone)
- `-k` : Keep TrustZone active

### `c_cpp_properties.json` — IntelliSense

Configuré pour le sous-projet AppliNonSecure avec les defines `STM32N657xx`,
`USE_HAL_DRIVER`, et le compilateur GCC 14.3.

---

## Pièges connus

| Piège | Symptôme | Solution |
|-------|----------|----------|
| CubeIDE ouvert | Flash semble réussir mais rien ne fonctionne | Fermer CubeIDE avant de flasher depuis VS Code |
| `monitor reset` en GDB | CPU repart dans le boot ROM (0x18xxx) | Ne pas utiliser reset ; recharger le ELF avec `load` |
| Mauvais Access Port | GDB server : "Fail starting session" | Utiliser `-m 1` (AP 1 = Cortex-M55) |
| CubeProgrammer v2.20 | GDB server incompatible | Utiliser v2.22 bundled avec CubeIDE 2.1.1 |
| `--freq` / `--serial` | Parse error dans GDB server v7.13 | Syntaxe v7.13 : `--frequency` et `-i` |

---

## ST-LINK

- **Firmware** : V3J17M10
- **SN** : 004200253234511233353533
- **SWD** : 8000 kHz
- **BOOT1** : positions 1-3 (mode développement, boot depuis ROM interne)
