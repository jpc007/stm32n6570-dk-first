# État du projet « first » — STM32N6570-DK

**Dernière mise à jour : 2026-04-18 (session 16)**

## ÉCRAN LCD ROUGE — FONCTIONNEL ✅ (Phase 1 / FSBL) | Phase 2 TrustZone — ÉCRAN NOIR ❌ (AppliSecure path) | Chaîne NS — Chenillard + UART ✅ (après fix HAL) | GDB DEBUG — FONCTIONNEL ✅ | VS Code IntelliSense — FONCTIONNEL ✅ | Dual-IDE Build — FONCTIONNEL ✅

**Dernières évolutions (session 16 du 2026-04-18) :**
- **Écran rouge « de retour » en Phase 2a** : le FSBL affiche toujours le plein écran **ROUGE** (FB RGB565 `0xF800`), **pause ~3 s**, trace `LCD HAL ref …`, puis saut vers AppliSecure. Ce comportement n’a pas été retiré — la régression visuelle signalée concernait surtout la **suite NS** (écran noir **après** handoff Secure) ou l’absence de perception d’image une fois en AppliNonSecure.
- **HardFault / SecureFault juste après `[NS] main() reached!`** : le premier `HAL_Init()` côté **NonSecure** appelait `HAL_MspInit()` généré par CubeMX, qui exécutait `HAL_PWREx_EnableVddIO2..5()` → accès aux registres **PWR** (`SVMCRx`, plage ~`0x0701xxxx`) alors que **PWR reste typiquement Secure (RIF)** alors que le **RCC** a été largement rendu accessible au NS via **`RCC_S->PUBCFGR*`** dans `SystemIsolation_Config()`. Résultat : **SecureFault** (ex. `SFSR=0x48` avec `SFARVALID` + `AUVIOL`, `SFAR`/`MMFAR` cohérents avec PWR), LEDs/chenillard jamais atteints.
- **Correctif AppliNonSecure** (`stm32n6xx_hal_msp.c`) : **suppression** des `HAL_PWREx_EnableVddIOx()` dans `HAL_MspInit()` (déjà effectués côté AppliSecure avant le saut NS ; redondants et dangereux en NS).
- **USART2 côté NS** : suppression de `HAL_RCCEx_PeriphCLKConfig()` dans `HAL_UART_MspInit()` — le **kernel clock USART2 (CLKP)** est déjà configuré par le **FSBL** ; évite des accès **CCIPR / `LL_RCC_SetClockSource`** depuis le NS (risque RIF / attributs même avec PUBCFGR partiel).
- **Chenillard** : après un essai à **60 ms** (jugé « hyper accéléré »), pas remis à 150 ms mais à **~120 ms** par LED (`main.c` NS) pour un rythme lisible tout en restant plus vif que l’original 150 ms.
- **LEDs boot AppliSecure** (jaune PE9 / rouge PH5) : délais **500 ms → 200 ms** chacun (`AppliSecure/Core/Src/main.c`, USER stage 2) — intention : même idée de **pause observable** que l’écran rouge FSBL, **plus courte** sur les GPIO LED.
- **Documentation** : cette session est consignée ici ; message Git en anglais : *Red boot screen retained; NonSecure HAL_Init TZ fix; LED chase timing; doc*.

**Dernières évolutions (session 15 du 2026-04-18) :**
- **Fix définitif build dual-IDE (VS Code + CubeIDE)** : CubeMX ne génère PAS les règles de build pour `stm32n6xx_hal_rif.c` et `stm32n6xx_hal_ramcfg.c` dans `subdir.mk` malgré `HAL_RIF_MODULE_ENABLED` et `HAL_RAMCFG_MODULE_ENABLED` dans `hal_conf.h`. Modifier `subdir.mk` / `objects.list` manuellement fonctionnait pour VS Code mais était **écrasé à chaque build CubeIDE**. Ajouter des `<link>` dans `.project` était **ignoré** par CubeIDE (ne relit pas `.project` modifié en externe).
- **Solution pérenne** : création de `FSBL/makefile.defs` — inclus automatiquement par `Debug/makefile` via `-include ../makefile.defs`, fichier **jamais écrasé** par CubeIDE. Contient les sources/objets/règles de build pour `hal_rif.c` et `hal_ramcfg.c` avec gardes conditionnelles (`ifeq ($(filter %filename.o,$(OBJS)),)`) pour éviter les doublons si CubeIDE les ajoute un jour. Les `.o` sont passés au linker via `USER_OBJS` (après `@"objects.list"`).
- **Résultat** : les 3 projets compilent identiquement depuis VS Code (tasks make) ET CubeIDE (Clean+Build). FSBL=51912 bytes, AppliSecure=21156 bytes, AppliNonSecure=331948 bytes.
- **Warnings linker GCC 14.3** (non bloquants) : « LOAD segment with RWX permissions » et « missing .note.GNU-stack section implies executable stack » — normaux sur MCU bare-metal sans MMU, liés à la toolchain GCC 14.3.1. CubeIDE les affiche comme "C/C++ Problem" mais ce sont des warnings inoffensifs.

**Évolutions précédentes (session 14 du 2026-04-13, soir) :**
- **BUG CRITIQUE TROUVÉ : PFCR pixel format erroné** (`layer->PFCR = 2U`). Sur STM32N6, l'encodage LTDC PFCR est **différent** des anciens STM32 (F4/F7/H7). Le N6 ajoute 4 formats byte-swapped (ABGR8888, RGBA8888, BGRA8888, BGR565) aux positions 1-5, décalant RGB565 de la position 2 à la position **4**. Le code écrivait `PFCR=2` = **RGBA8888** (4 bytes/pixel) au lieu de `PFCR=4` = **RGB565** (2 bytes/pixel). Le LTDC interprétait le framebuffer 16-bit comme du 32-bit → pitch incorrect + pixels potentiellement transparents → **écran noir**.
- **Mapping PFCR N6 (confirmé via HAL `stm32n6xx_hal_ltdc.h`)** : 0=ARGB8888, 1=ABGR8888, 2=RGBA8888, 3=BGRA8888, **4=RGB565**, 5=BGR565, 6=RGB888, 7=Flexible (FPF0R/FPF1R). La description CMSIS dans `stm32n657xx.h` est **OBSOLÈTE** (montre l'ancien mapping F4 : 010=RGB565).
- **Fix appliqué** : `layer->PFCR = 4U;` dans `AppliSecure/Core/Src/main.c` (section 7c). Ajout diagnostic `[7c] PFCR=` readback.
- **RIMC bit 31 "DMAEN" n'existe PAS** sur STM32N6 : aucune définition `RIFSC_RIMC_ATTRx_DMAEN` dans les headers CMSIS. Le `|= (1UL<<31)` était silencieusement ignoré par le HW. Code mort supprimé.
- **CORRECTION analyse RIMC_LTDC1=0x00000310** : le décodage session 12 était **faux**. Valeur correcte : **MCID=1** (bits[6:4]=001 ✅), MSEC=1 (bit8 ✅), MPRIV=1 (bit9 ✅). Le CID EST bien 1, pas 0. Les exemples BSP officiels ne positionnent pas de bit 31 non plus.
- **Écran toujours NOIR après le fix PFCR** (testé). PFCR shadowed : readback=0 avant reload IMR, valeur finale non vérifiée après reload. BCCR vert toujours invisible. ISR=0, CPSR scanne OK. GCR=0x10002221 (BCKEN bit17=0 malgré écriture — possiblement réservé sur N6). LEDs et chenillard OK.
- **cSpell** : ~150 warnings supprimés. `cspell.json` créé avec ~120 mots STM32/HAL + `ignorePaths` (*.md, Drivers/**, etc.). Commentaire français `Error_Handler` FSBL traduit en anglais. Commit `99ff937` poussé sur GitHub.
- **Pistes restantes pour l'écran noir** : (1) Utiliser l'approche HAL (`HAL_LTDC_Init` + `HAL_LTDC_ConfigLayer`) au lieu de CMSIS direct — c'est ce qui fonctionne en Phase 1. (2) Le pixel clock utilise PLL1/45=26.67 MHz au lieu du BSP PLL4/2=25 MHz — tester avec PLL4. (3) Ajouter diagnostic PFCR post-reload. (4) Vérifier FPF0R/FPF1R (registres Flexible Pixel Format, doivent être 0). (5) Vérifier si BCCR nécessite un reload sur N6.

**Évolutions précédentes (session 13 du 2026-04-13) :**
- **Fix linker errors FSBL** : `HAL_RIF_RIMC_ConfigMasterAttributes`, `HAL_RIF_RISC_SetSlaveSecureAttributes`, `HAL_RAMCFG_EnableAXISRAM` étaient `undefined reference`. CubeMX n'avait pas ajouté `stm32n6xx_hal_rif.c` ni `stm32n6xx_hal_ramcfg.c` au build malgré `HAL_RIF_MODULE_ENABLED` et `HAL_RAMCFG_MODULE_ENABLED` dans `hal_conf.h`.
- **Fix appliqué** : ajout des 2 fichiers dans `FSBL/Debug/Drivers/STM32N6xx_HAL_Driver/subdir.mk` (C_SRCS + OBJS + C_DEPS + règles de build) ET dans `FSBL/Debug/objects.list` (manquait au 1er essai → 2ème tentative réussie). FSBL compile : 51788 bytes code.
- **Fix IntelliSense VS Code** : 124 erreurs "identifier is undefined" (`I2C_HandleTypeDef`, `UART_HandleTypeDef`, `ADC_HandleTypeDef`, etc.) dans `FSBL/Core/Src/main.c`. Cause : l'extension C/C++ ne résolvait pas la chaîne d'includes HAL (hal.h → hal_conf.h → hal_i2c.h etc.) sans les bons flags de compilation.
- **Solution** : génération automatique de `compile_commands.json` (904 entrées, 3 sous-projets) via `make -nB` dry-run. Ajout de `"compileCommands": "${workspaceFolder}/compile_commands.json"` dans les 3 configurations de `.vscode/c_cpp_properties.json`. Ajout de `-mcmse` au FSBL config. Reset IntelliSense database → **0 erreurs**.
- **Note** : si les Makefiles changent (ajout/suppression de fichiers), il faut regénérer `compile_commands.json` via la même commande `make -nB`.

**Évolutions précédentes (session 12 du 2026-04-06, soir) :**
- **Phase 2 TrustZone réactivée** dans AppliSecure : LTDC init complète côté Secure (section 7), CFBAR=0x34200000 (alias S), RIMC LTDC SEC+PRIV+CID1.
- **Découverte RISAF mapping** : RISAF4/RISAF5 = ports NPU uniquement (4 GB, pas le CPU AXI). SRAM3 CPU AXI **n'a PAS de RISAF dédié** — l'accès NS est bloqué par le NoC/interconnect, pas par RISAF. RISAF4 IASR=0 confirme.
- **Approche SEC+PRIV** : RIMC LTDC1/LTDC2 = SEC+PRIV, RISC LTDC/L1/L2 = SEC+PRIV, CFBAR = 0x34200000 (alias Secure). LTDC reste SEC (pas de bascule NSEC).
- **Diagnostics GPIO** : PB13(CLK)=AF14 ✅, PE11(VSYNC)=AF14 ✅, PG13(DE)=OUT HIGH ✅, PQ3+PQ6=HIGH ✅.
- **ISR=0** (aucune erreur DMA), **CPSR change** (LTDC scanne) — mais écran **toujours noir**, même le BCCR vert n'est pas visible.
- **RIMC_LTDC1 = 0x00000310** → initialement analysé comme DMAEN=0/CID=0 (session 12). **Corrigé session 14** : décodage correct = MCID=1(bits[6:4]=001 ✅), MSEC=1(bit8 ✅), MPRIV=1(bit9 ✅). Le bit 31 « DMAEN » n'existe pas dans le registre RIMC_ATTRx sur STM32N6. Code `|= (1UL<<31)` supprimé.

**Évolutions précédentes (session 11 du 2026-04-06) :**
- **CubeIDE 2.1.1 découvert** : installé dans `C:\ST\STM32CubeIDE_1.13.0\` (nom de dossier trompeur, confirmé via `stm32cubeide.ini`). Contient GCC 14.3.1, GDB server 7.13.0, CubeProgrammer v2.22.0.
- **GDB debug fonctionne** avec GDB server 7.13 + AP 1 (`-m 1`) + connect under reset (`-k`). L'ancien GDB server 7.1 (CubeIDE 1.11) ne supporte pas le N6.
- **Attention flash** : le `-d file.elf` du CubeProgrammer peut charger le fichier ELF brut au lieu de parser les sections. La tâche tasks.json (mode=UR + coreReg + -s) reste la méthode fiable.
- **CubeIDE ouvert bloque le ST-LINK** : si STM32CubeIDE est lancé, les commandes Programmer CLI semblent réussir mais rien ne se passe sur la carte (pas de clignotement LED programmeur). Toujours fermer CubeIDE avant de flasher depuis VS Code.

**Évolutions précédentes (session 10, 2026-04-06, soir) :**
- **LCD ROUGE AFFICHÉ** — après 9 sessions d'écran noir, l'écran RK050HR18 affiche enfin du rouge. Trace USART complète confirmée :
  ```
  === PHASE 1 : FSBL LCD TEST ===
  [OK] Security_Config
  [OK] SRAM3+SRAM4 enabled
  [OK] LTDC clock = 25 MHz
  [LTDC] HAL_LTDC_Init OK
  [LTDC] Layer 0 configured
  [LTDC] Framebuffer filled RED
  === LCD should show RED ===
  ```
- **Stratégie gagnante** : approche **FSBL Secure-only** (pas de TrustZone split). Tout le code LCD tourne en mode Secure dans le FSBL, sans saut vers AppliSecure/AppliNonSecure. Basé sur les exemples officiels ST qui n'utilisent **jamais** TrustZone + LCD ensemble.
- **Clé du succès** : PLL4 (HSE/4×50=600MHz) → IC16/24 = **25 MHz pixel clock** (identique au BSP), HAL LTDC/RIF/RAMCFG, Security via `HAL_RIF_RIMC` + `HAL_RIF_RISC`, framebuffer en SRAM3 Secure (0x34200000).
- **Boot via VS Code** : séquence 3 étapes STM32_Programmer_CLI (download ELF → coreReg MSP+PC → start). CubeIDE non utilisé. BOOT1=1-3 (dev mode), flash externe effacée.
- **LED verte clignote** : boucle infinie confirmée par dump registres (PC dans notre code, SP correcte).

**Évolutions précédentes (sessions 1-8, résumé) :**
- Sessions 1-4 : init LTDC CMSIS + TrustZone split → toujours noir
- Session 5 : découverte CFBAR=0 (NS ignoré), fix GRMSK, LTDC init côté Secure
- Session 6 : FSBL bloqué par `Error_Handler` (MX_xxx_Init), fix 12 `return;`
- Session 7-8 : LED OK, changement stratégie → Phase 1 FSBL-only → **LCD ROUGE**

Ce document centralise toute la connaissance acquise sur le projet. Il permet à un assistant sans historique de reprendre le travail immédiatement.

---

## 1. Architecture actuelle : FSBL Secure-only (Phase 1)

**Mode actuel** : le projet fonctionne en mode **FSBL-only** (Secure). Pas de saut vers AppliSecure ni AppliNonSecure. Tout le code LCD tourne dans le FSBL.

| Projet | État | Rôle |
|--------|------|------|
| **first_FSBL** | **ACTIF — LCD fonctionnel** | Init horloges (PLL1 1200 MHz + PLL4 600 MHz), GPIO, Security (RIMC+RISC), SRAM3/4, LTDC 25 MHz, HAL_LTDC_Init, Layer RGB565, FB RED, LED blink |
| **first_AppliSecure** | **ACTIF — Phase 2 en cours** | Config TrustZone + LTDC init complète (section 7), RIMC/RISC SEC+PRIV, CFBAR=0x34200000 (S alias). Écran noir — RIMC DMAEN/CID suspects |
| **first_AppliNonSecure** | **ACTIF — chenillard + diagnostic** | Après fix MSP NS : `HAL_Init` OK, LEDs chenillard (~120 ms/LED), USART2, NSC `SECURE_LtdcFbFillRgb565` si `NS_DIAG_NO_LVGL` ; écran peut rester noir selon phase Secure (voir §5) |

### Boot FSBL (Phase 1)

Séquence dans `FSBL/Core/Src/main.c` :
1. **USER CODE 1** : LED VERTE via CMSIS direct (PD0, avant `HAL_Init`)
2. `HAL_Init()` → `SystemClock_Config()` (PLL1 + **PLL4**) → `PeriphCommonClock_Config()`
3. `MX_GPIO_Init()` puis 12 `MX_xxx_Init` (tous avec `return;` en USER CODE Init 0)
4. **USER CODE 2** :
   - LED VERTE init HAL (PD0)
   - `USART2_Init()` (PD5 AF7, 115200)
   - `Security_Config()` — RIMC (LTDC1/LTDC2 = SEC+PRIV+CID1), RISC (LTDC/L1/L2 = SEC+PRIV)
   - `SRAM_Enable()` — RAMCFG pour SRAM3+SRAM4 (AXI)
   - `LTDC_ClockConfig()` — IC16 = PLL4/24 = 25 MHz pixel clock
   - `LTDC_Init_Phase1()` → `HAL_LTDC_Init()` (appelle `HAL_LTDC_MspInit` : 28 GPIOs + contrôle) → `HAL_LTDC_ConfigLayer` (RGB565 800×480, FB @ 0x34200000) → remplissage RED (0xF800)
5. Boucle infinie : LED toggle 500ms

### Comment flasher (VS Code / CLI)

**Séquence 3 étapes** avec STM32_Programmer_CLI v2.20.0 :
```powershell
# 1. Download ELF (Under Reset)
STM32_Programmer_CLI -c port=SWD sn=<SN> mode=UR reset=HWrst -d FSBL/Debug/first_FSBL.elf

# 2. Set CPU registers (HotPlug)
STM32_Programmer_CLI -c port=SWD sn=<SN> mode=HotPlug -coreReg MSP=0x34200000 PC=<entry>

# 3. Start execution (HotPlug)
STM32_Programmer_CLI -c port=SWD sn=<SN> mode=HotPlug -s
```
- `<entry>` = Reset_Handler, obtenu via `arm-none-eabi-objdump -f first_FSBL.elf`
- `<SN>` = `004200253234511233353533`
- **Pourquoi 3 étapes** : `-s <addr>` seul ne positionne pas le MSP → le boot ROM reprend la main
- **BOOT1** doit être en position **1-3** (dev mode, boot ROM halted)
- Flash externe **effacée** (pas de factory demo)

### Architecture TrustZone Phase 2 (en cours — session 12)

La chaîne FSBL → AppliSecure → AppliNonSecure fonctionne pour les **LEDs + USART2 + chenillard** une fois le fault NS corrigé (session 16). L'init LTDC complète est faite côté Secure dans AppliSecure (section 7, registres CMSIS via alias `_S`). Le LTDC reste SEC+PRIV (pas de bascule NSEC). L'écran reste **noir** en Phase 2 sur le chemin AppliSecure (voir §5.1 ; **le rouge FSBL Phase 2a reste OK**).

---

## 2. TrustZone / RIF — Configuration de sécurité

### 2.1 Phase 2a actuelle : FSBL boot chain (`FSBL/Core/Src/main.c`)

Le FSBL fait maintenant un boot chain Phase 2a (init + jump to AppliSecure) :

- **LED VERTE** (PD0) : allumée CMSIS direct avant `HAL_Init`, puis réinit HAL
- **USART2** : PD5 AF7, 115200, TX-only pour traces debug
- **Security_Config()** : `HAL_RIF_RIMC_ConfigMasterAttributes` (LTDC1/LTDC2 = CID1+SEC+PRIV), `HAL_RIF_RISC_SetSlaveSecureAttributes` (LTDC/L1/L2 = SEC+PRIV)
- **SRAM_Enable()** : `HAL_RAMCFG_EnableAXISRAM` pour SRAM2+SRAM3+SRAM4 (AXI)
- **Jump_To_AppliSecure()** : lit MSP+Reset_Handler à 0x34000400, valide, positionne VTOR+MSP, saute
- **Note build** : `stm32n6xx_hal_rif.c` et `stm32n6xx_hal_ramcfg.c` ajoutés manuellement dans subdir.mk et objects.list (session 13 — CubeMX ne les avait pas inclus)

### 2.2 Phase 2 (EN COURS — session 12) : AppliSecure + AppliNonSecure

Le code TrustZone split est **actif** dans `AppliSecure/Core/Src/main.c`. La chaîne FSBL → AppliSecure → AppliNonSecure fonctionne (LEDs + USART2 OK), mais l'écran LCD reste noir.

**Config RIF (USER CODE RIF_Init 2)** :
- **RISC (section 1)** : USART2 = NSEC+NPRIV. **LTDC/LTDCL1/LTDCL2 = SEC+PRIV** (changé session 12, avant : NSEC+NPRIV)
- **RIMC (section 2)** : LTDC1/LTDC2 = CID1 + **SEC+PRIV** (session 12). Ancienne tentative « bit 31 DMAEN » : **supprimée session 14** — ce bit **n’existe pas** dans `RIMC_ATTRx` sur STM32N6. Readback `RIMC_LTDC1=0x00000310` = **MCID=1, MSEC=1, MPRIV=1** (décodage session 14).
- **GPIO NSEC** : toutes les broches LCD (PA15, PB4, PB13, PE1, PG0 ajoutées manuellement car CubeMX les oublie), les 5 LEDs debug (PD0, PE9, PH5, PE10, PE13), GPIOQ (PQ3, PQ6)
- **SRAM3/4** : clocks + shutdown (retry si FSBL n'a pas pris effet)
- **RISAF4 (section 7d1)** : Region 0 full range (0x00000000–0xFFFFFFFF), tous CIDs autorisés, NSEC. **Note** : RISAF4 = port NPU 0, pas le CPU AXI SRAM3 (voir §2.3).
- **NPU clock** : `__HAL_RCC_NPU_CLK_ENABLE()` activée (section 5d) car RISAF4/5 sont sur le bus NPU
- **LTDC clocks** : IC16, CCIPR4, APB5, reset (mêmes valeurs que FSBL)
- **PUBCFGR** : IC16PUB, PERPUB, APB5PUB, AXISRAM3/4PUB
- **MPU NS** : region 7, 0x24200000 (768 KB), NON_CACHEABLE, OUTER_SHAREABLE, ALL_RW
- **Section 7 — LTDC FULL INIT FROM SECURE** :
  - GPIO AF14 pour toutes les broches LCD (identique au BSP `LTDC_MspInit`)
  - Sorties PP : PQ3 (LCD_ON) HIGH, PQ6 (BL) HIGH, PG13 (DE, pull-up) HIGH
  - PE1 : cycle reset panneau (LOW 2ms → HIGH 120ms via `HAL_Delay`)
  - LTDC registres via `LTDC_S` / `LTDC_Layer1_S` : timings RK050HR18, layer 1 RGB565, **`CFBAR=0x34200000`** (alias Secure, changé session 12), `CFBLR`, `CFBLNR`, `CACR=0xFF`, `BFCR` PAxCA
  - Remplissage FB blanc via pointeur volatile sur 0x34200000 (alias S, données OK : `FB@S=0xA5A5`)
  - Enable (`LEN` + `LTDCEN`) + **`SRCR_IMR`** (reload immédiat, **sans GRMSK**)
  - **Section 7f2** : diagnostic BCCR vert (0x0000FF00), ISR/CPSR readback → ISR=0, CPSR scanne OK
  - **Section 7f3** : diagnostic GPIO readback (tous AF14/OUTPUT OK) + RIMC_LTDC1 readback (0x00000310)
  - **Section 7g** : LTDC **reste SEC** (pas de bascule NSEC). AppliNonSecure accèderait via NSC veneers si nécessaire.
- **NonSecure_Init()** : écrit `VTOR_NS = SRAM2_AXI_BASE_NS + 0x400`, positionne MSP_NS, saute vers le Reset_Handler NS

### 2.3 Points critiques TrustZone (leçons apprises)

- Les registres `RCC->IC16CFGR`, `DIVENSR`, `CCIPR4`, `APB5ENSR` sont **Secure-only** : les écritures NS sont **silencieusement ignorées**.
- **Les registres LTDC eux-mêmes sont silencieusement ignorés depuis NS** malgré `RISUP NSEC` pour LTDC/L1/L2. Preuve : `LTDC_Layer1_NS->CFBAR` relu = `0x00000000` après écriture de `0x24200000`. **Toute la config LTDC doit se faire depuis Secure** (alias `_S`).
- PB13 (LCD_CLK) doit être **NSEC** dans AppliSecure, sinon le `HAL_GPIO_Init(AF14)` NS est silencieusement ignoré → pas de pixel clock → écran noir.
- Le STM32N6 RCC utilise des **SET registers** (`xxxENSR`) pour activer les clocks : écrire dans `xxxENR` directement ne fonctionne **pas**.
- `RCC_PRIVCFGR5` n'existe pas sur le STM32N6 (PRIVCFGR0..4 uniquement). Seul `PUBCFGR5` existe.
- Le bit **GRMSK** (bit 2) du registre `LTDC_LxRCR` **masque le reload global** depuis `LTDC_SRCR`. Si GRMSK=1, `SRCR_VBR` et `SRCR_IMR` sont sans effet pour ce layer → les shadow registers ne se chargent jamais → registres actifs (CFBAR etc.) = valeur reset (0).
- **`RCC->MEMENR` lu depuis NS** retourne 0 si les bits PUBCFGR correspondants ne sont pas positionnés. Le diagnostic `SRAM3 clock OFF` était un **faux négatif**. Ne pas se fier à `RCC_NS->MEMENR` pour vérifier les clocks mémoire depuis le monde NS.
- **`Error_Handler()` par défaut CubeMX** contient `while(1)`. Dans un FSBL, cela **bloque toute la chaîne de boot** si un seul `MX_xxx_Init` échoue (ex. SDMMC sans carte SD). Solution : rendre `Error_Handler` non-bloquant dans le FSBL + skipper les `MX_xxx_Init` inutiles au boot.
- **(Session 12) RISAF4 et RISAF5 = ports NPU uniquement** (existent seulement si `NPU_PRESENT` est défini). Espace d'adressage 4 GB (pas 1 MB comme RISAF2/3). **SRAM3 CPU AXI n'a PAS de RISAF dédié**. L'accès NS à SRAM3 (0x24200000) est bloqué par le **NoC/AXI interconnect**, pas par RISAF. Mapping complet : RISAF2=AXISRAM1(1MB), RISAF3=AXISRAM2(1MB), RISAF4=NPU port 0(4GB), RISAF5=NPU port 1(4GB), RISAF6=NoC(4GB), RISAF7=FLEXRAM(512KB).
- **(Session 12→14) RIMC_LTDC1 readback = 0x00000310** : **décodage CORRIGÉ** session 14 → MCID=1 (bits[6:4]=001 ✅), MSEC=1 (bit8 ✅), MPRIV=1 (bit9 ✅). L'analyse session 12 avec « CID=0 » et « DMAEN=0 » était **fausse**. Le bit 31 « DMAEN » n'existe **PAS** dans le registre `RIFSC_RIMC_ATTRx` du STM32N6 (aucune définition dans les headers CMSIS, aucun exemple BSP ne le positionne). Configuration RIMC = correcte.
- **(Session 14) PFCR pixel format encodage N6** : l'IP LTDC du STM32N6 a un encodage PFCR **différent** des F4/F7/H7. Le registre `LTDC_LxPFCR.PF` (3 bits) supporte 7 formats prédéfinis + 1 mode flexible : 0=ARGB8888, 1=ABGR8888(NEW), 2=RGBA8888(NEW), 3=BGRA8888(NEW), 4=RGB565(DÉPLACÉ), 5=BGR565(NEW), 6=RGB888(DÉPLACÉ), 7=Flex(FPF0R/FPF1R). La description CMSIS `stm32n657xx.h` est **OBSOLÈTE** (liste l'ancien mapping). Seul le HAL (`stm32n6xx_hal_ltdc.h`) a les bonnes valeurs.

---

## 3. Memory map (linker)

| Région | Origine | Taille | Usage |
|--------|---------|--------|-------|
| **ROM** (NS) | 0x24100400 | 511K | Code + constantes AppliNonSecure (SRAM2) |
| **RAM** (NS) | 0x24180000 | 512K | Data + BSS + heap (4K) + stack (8K) |
| **FBRAM** | 0x24200000 | 768K | Framebuffer LTDC (section `.framebuffer`, SRAM3) |
| **Secure ROM** | 0x34000400 | 399K | AppliSecure |
| **Secure RAM** | 0x34064000 | 624K | AppliSecure data |
| **FSBL ROM** | 0x34180400 | 255K | FSBL |

**SAU (`AppliSecure/Core/Inc/partition_stm32n657xx.h`)** :
- Region 0 : NS SRAM2 (code+data) 0x24100000 – 0x241FFFFF
- Region 1 : NS SRAM3+SRAM4 (framebuffer) 0x24200000 – 0x242DFFFF
- Region 2 : **NSC** — plage **`.gnu.sgstubs` (CMSE)** 0x34000760 – 0x3400077F (obligatoire : sans cela le premier appel NSC provoque **INVEP** / faute sécurisée car les veneers SG ne sont pas en région NSC)
- Region 3 : NS peripherals 0x40000000 – 0x4FFFFFFF

**Note** : le framebuffer est en **SRAM3** (0x24200000). Il avait été temporairement déplacé en SRAM1 (session précédente) mais est revenu en SRAM3 car RISAF4 est correctement ouvert et la SAU Region 1 couvre SRAM3/4. Le code `lv_port_disp_ltdc.c` place le buffer via `__attribute__((section(".framebuffer")))` → FBRAM. Le remplissage initial (blanc) est fait par AppliSecure via pointeur volatile sur 0x24200000.

---

## 4. LVGL — État d'intégration

### 4.1 Sources et configuration

| Élément | Chemin | État |
|---------|--------|------|
| Bibliothèque LVGL | `Middlewares/lvgl/` | **Junction** vers `C:\Develop\libs\lvgl` (créé via `mklink /J`, partagé sans copie) |
| Config LVGL | `AppliNonSecure/Core/Inc/lv_conf.h` | Configuré : `LV_COLOR_DEPTH 16`, `LV_MEM_SIZE 96K`, `LV_USE_ST_LTDC 0`, widgets BUTTON+LABEL+IMAGE activés, démos désactivées |
| Port display | `AppliNonSecure/GUI/lvgl_port/lv_port_disp_ltdc.c` | **Ne programme plus le LTDC** (fait par Secure). Crée uniquement le display LVGL, mode partial (20 lignes, double buffer render). Le `disp_flush_cb` écrit dans le framebuffer SRAM3 (0x24200000). |
| Port tick | `AppliNonSecure/GUI/lvgl_port/lv_port_tick.c` | Exists |
| Facade | `AppliNonSecure/GUI/lvgl_port/lvgl_port.h` | `lv_port_init()`, `lv_port_tick_inc()` |

### 4.2 Écran LCD

- **Panneau** : RK050HR18-CTG (MB1860-B01), 800×480 RGB parallèle 24 bits, connecteur CN3
- **Résolution** : 800 × 480
- **Format pixel** : RGB565
- **Timings LTDC** : HSYNC=4, HBP=4, HFP=4, VSYNC=4, VBP=4, VFP=4 (conformes BSP V1.0.1)
- **Pixel clock** : IC16 = PLL1 (1200 MHz) / 45 = 26.67 MHz (BSP utilise PLL4/2 = 25 MHz)
- **PCPOL** : inversé (GCR bit 28), panneau capture sur front descendant
- **LCD_DE (PG13)** : GPIO OUTPUT PP HIGH (statique, **pas** AF14 — AF14 provoque écran noir car le LTDC met DE à LOW pendant le blanking)
- **LCD power** : PQ3 HIGH (LCD_ON), PQ6 HIGH (backlight), PE1 cycle reset (LOW 2ms → HIGH 120ms)
- **I2C (touch)** : 0xBA/0xBB sur I2C2 (PD4 SDA, PD14 SCL) — FT5336, non intégré pour l'instant

### 4.3 Appels dans main.c (AppliNonSecure)

```c
/* LCD/LTDC déjà initialisé par AppliSecure (FB blanc en SRAM3). */
lv_port_init();  /* → lv_init() + lv_display_create() (PAS de ltdc_hw_init) */
/* Diagnostic LTDC : lecture CFBAR, GCR, CR, WHPCR + pixels FB */

/* Boucle principale : */
while(1) {
    lv_timer_handler();  /* LVGL task handler */
    /* + chenillard + Hello World */
}
```

`lv_refr_now()` a été retiré (bloquait le boot). Le `lv_timer_handler()` s'occupe du rendu LVGL.

### 4.4 LTDC init détaillée (dans `AppliSecure/Core/Src/main.c`, section 7)

Le driver LTDC est piloté par registres CMSIS via les alias **Secure** (`LTDC_S`, `LTDC_Layer1_S`) car :
1. Les écritures NS vers `LTDC_NS` sont **silencieusement ignorées** (voir §2.3)

Points clés :
- **RCR = 0** : ne **JAMAIS** mettre **GRMSK** (bit 2). GRMSK masque le reload global (`SRCR`) → les shadow registers ne se chargent pas → registres actifs = valeurs reset (0). C'était la cause du CFBAR=0.
- **Reload** : utiliser `LTDC_LxRCR_IMR` (per-layer immediate reload), attendre que le HW efface le bit.
- **CFBLR** : `line_length = width * bytes_per_pixel + 7` (et non +3 comme les anciens STM32).
- **Séquence** : `LEN` → `LTDCEN` → `RCR=IMR` → attente.
- **(Session 14) PFCR** : écrire **4** pour RGB565 sur N6, **PAS 2** (l'ancien mapping F4/F7 est obsolète). Voir §2.3 pour le mapping complet.
- **Framebuffer** : rempli en blanc (`0xFFFF` RGB565) par Secure avant enable. Adresse **0x34200000** (alias Secure, changé session 12 — avant 0x24200000). Données confirmées : `FB@S[0]=0xA5A5` (pattern test).
- **CFBAR = 0x34200000** (alias Secure) car le DMA LTDC est configuré SEC+PRIV (session 12). L'alias NS 0x24200000 n'est pas accessible au DMA LTDC en mode SEC.

---

## 5. LCD / LTDC — Phase 1 FONCTIONNEL ✅ | Phase 2 ÉCRAN NOIR ❌

### 5.1 État actuel

**Phase 1 (FSBL-only)** : l'écran affiche du ROUGE (RGB565 0xF800). Confirmé par trace USART et visuellement. ✅

**Phase 2 (TrustZone, session 12→14)** : écran **NOIR** ❌. L'init LTDC est faite côté Secure dans AppliSecure (section 7). ISR=0 (pas d'erreur DMA), CPSR change (LTDC scanne). BCCR vert (pas de DMA) n'est **pas visible**. GPIO vérifiés OK.

**Bug trouvé session 14** : `PFCR=2` au lieu de `PFCR=4` pour RGB565 (encodage N6 différent des anciens STM32). Fix appliqué, **écran toujours noir**. RIMC_LTDC1=0x00000310 = correct (CID=1, SEC, PRIV — l'analyse session 12 « CID=0 » était fausse). Le bit 31 « DMAEN » n'existe pas sur N6.

**Analyse en cours** : GCR=0x10002221 montre BCKEN(bit17)=0 malgré écriture. Le BCCR vert (background, sans DMA) reste invisible → suspicion que le timing/pixel clock ne sort pas du périphérique, ou que l'init CMSIS manque des registres que HAL_LTDC_Init configure. **Recommandation forte : basculer sur HAL_LTDC_Init** (approche Phase 1 qui fonctionne).

### 5.2 Configuration LTDC fonctionnelle

| Paramètre | Valeur |
|-----------|--------|
| Pixel clock | IC16 = PLL4(600MHz) / 24 = **25 MHz** |
| PLL4 | HSE(48MHz) / 4 × 50 = 600 MHz (ajouté dans `SystemClock_Config`) |
| Timings | HSYNC=4, HBP=4, HFP=4, VSYNC=4, VBP=4, VFP=4 (BSP V1.0.1) |
| Polarités | HS_AL, VS_AL, DE_AL, PC_IPC |
| Format pixel | RGB565 |
| Framebuffer | 0x34200000 (SRAM3, alias Secure) |
| Taille FB | 800 × 480 × 2 = 768 000 octets |
| Init | `HAL_LTDC_Init` + `HAL_LTDC_ConfigLayer` (HAL standard) |
| GPIO | 28 pins AF14 (SPEED_HIGH) + PE1 reset + PQ3 LCD_ON + PG13 DE (OUTPUT HIGH) + PQ6 backlight |
| Security | RIMC LTDC1/LTDC2 = CID1+SEC+PRIV, RISC LTDC/L1/L2 = SEC+PRIV |

### 5.3 Causes racines du black screen (toutes résolues)

| # | Cause | Solution |
|---|-------|----------|
| 1 | **TrustZone split** : écritures NS vers LTDC silencieusement ignorées | **Tout l'init LTDC en mode Secure** (FSBL-only, Phase 1) |
| 2 | **Pixel clock PLL1/45=26.67MHz** au lieu de PLL4/24=25MHz | **PLL4 activé** dans SystemClock_Config, IC16=PLL4/24 |
| 3 | **PB13 (LCD_CLK) pas NSEC** en mode TrustZone | Non applicable en Phase 1 (tout Secure) |
| 4 | **RISAF4 privilege mismatch** : LTDC DMA bloqué | Non applicable (HAL RIF SEC+PRIV) |
| 5 | **LCD_DE (PG13) en AF14** : LTDC mettait DE LOW pendant blanking | **GPIO OUTPUT PP HIGH** (statique) |
| 6 | **SRAM3/4 en shutdown** (SRAMSD=1 au reset) | `HAL_RAMCFG_EnableAXISRAM` |
| 7 | **FSBL bloqué par Error_Handler** dans MX_xxx_Init | `Error_Handler = __NOP()` + 12 `return;` |
| 8 | **GRMSK (bit 2 de LxRCR)** masquait le reload → CFBAR=0 | HAL_LTDC gère le reload correctement |
| 9 | **Boot mode flash** (BOOT1=1-2) : factory demo écrasait SRAM | **BOOT1=1-3** (dev mode) + flash externe effacée |
| 10 | **`-s <addr>` seul** ne positionne pas MSP | **Séquence 3 étapes** : download → coreReg → start |
| **11** | **(Session 14) PFCR=2 au lieu de 4 pour RGB565** : N6 a un encodage PFCR différent (4 nouveaux formats byte-swapped décalent les positions). PFCR=2 = RGBA8888 (4 B/px) | **Fix `PFCR=4`** (RGB565). Écran toujours noir → il y a d'autres problèmes |
| **12** | **(Session 14) Init CMSIS vs HAL** : les écritures CMSIS directes manquent potentiellement des registres que `HAL_LTDC_Init` configure (FPF0R/FPF1R, composition, BCKEN) | **Basculer sur HAL_LTDC_Init** (à tester session 15) |

### 5.4 Historique des sessions LCD

| Session | Action | Résultat |
|---------|--------|----------|
| 1 | Init LTDC basique CMSIS, framebuffer SRAM3 | Noir |
| 2 | Per-layer reload, LCD_DE GPIO, LCD_NRST reset, PCPOL | Noir |
| 3 | PB13/PE1/PG0 NSEC, RISAF4 open, RIMC LTDC NSEC | **Bleu** (fond) / Noir (layer) |
| 4 | Déplacement FB vers SRAM1, RISAF2 config | Noir (layer) |
| 5 | Découverte CFBAR=0 (NS ignoré), fix GRMSK, IMR reload | Noir |
| 6 | LTDC init complète dans AppliSecure (alias _S) | Noir |
| 7 | Séquence boot LED, LVGL junction | Noir (build stale) |
| 8 | FSBL bloqué par Error_Handler → fix 12 MX_xxx_Init | LED OK, LCD non testé |
| **9** | **Phase 1 FSBL-only : HAL LTDC, PLL4 25MHz, 3-step flash** | **ROUGE ✅** |
| 10 | LCD ROUGE confirmé (Phase 1), GDB debug fonctionnel avec CubeIDE 2.1.1 | **ROUGE ✅** |
| 11 | CubeIDE 2.1.1 découvert, GDB server 7.13 + AP 1, pièges documentés | **ROUGE ✅** |
| **12** | **Phase 2 TrustZone : RIMC SEC+PRIV, CFBAR=0x34200000, GPIO/RIMC diag** | **NOIR ❌** |
| **13** | **Fix build (hal_rif.c/hal_ramcfg.c manquants) + IntelliSense (compile_commands.json)** | **Build OK ✅, IntelliSense OK ✅** |
| **14** | **Fix PFCR 2→4 (RGB565 N6), suppression DMAEN bit 31, correction analyse RIMC, cSpell cleanup** | **NOIR ❌** (PFCR fixé mais d'autres problèmes) |

**Prochain test recommandé (session 15) :**
1. **PRIORITÉ 1 : Basculer sur `HAL_LTDC_Init` + `HAL_LTDC_ConfigLayer`** dans AppliSecure au lieu des écritures CMSIS directes. C'est l'approche qui fonctionne en Phase 1. Activer `HAL_LTDC_MODULE_ENABLED` dans `AppliSecure/Core/Inc/stm32n6xx_hal_conf.h`, ajouter `stm32n6xx_hal_ltdc.c` au build AppliSecure, et appeler le HAL depuis la section 7.
2. **Tester avec PLL4 pixel clock** (25 MHz exact) au lieu de PLL1/45 (26.67 MHz). La Phase 1 utilise PLL4. Vérifier si PLL4 est toujours configuré après le jump FSBL→AppliSecure (RCC persiste), sinon le reconfigurer.
3. **Ajouter diagnostic PFCR APRÈS le reload IMR** (actuellement seul le readback pré-reload est tracé, il montre 0 car c'est le registre actif pas encore mis à jour).
4. **Vérifier FPF0R et FPF1R** (Flexible Pixel Format registers) : doivent être = 0 pour les formats prédéfinis. Si non-zéro, ils interfèrent avec PFCR.
5. **Investiguer BCKEN (bit 17 GCR)** : la Phase 1 avec HAL_LTDC_Init ne positionne PAS BCKEN non plus (le HAL N6 ne le met pas). Vérifier si ce bit est fonctionnel ou réservé sur N6.
6. **Alternative radicale** : si HAL échoue aussi en Phase 2, le problème est dans la chaîne de boot (quelque chose que le FSBL Phase 1 fait que la séquence FSBL→AppliSecure ne fait pas). Comparer les registres RCC/GPIO/LTDC entre les deux cas.

---

## 6. Debug matériel — LEDs

### Phase 1 actuelle : une seule LED

En Phase 1 (FSBL-only), seule la LED VERTE (PD0) est utilisée :
- Allumée via CMSIS direct dans USER CODE 1 (avant `HAL_Init`) — confirme que le FSBL démarre
- Réinitialisée via HAL dans USER CODE 2
- **Clignote** en boucle infinie (toggle 500ms) — confirme que tout le code s'exécute

### Phase 2 (à réactiver) : 5 LEDs debug

5 LEDs soudées sur une carte de debug externe :

| Couleur | Port | Pin | Macro |
|---------|------|-----|-------|
| VERT | GPIOD | PD0 | `DBG_LED_GREEN` |
| JAUNE | GPIOE | PE9 | `DBG_LED_YELLOW` |
| ROUGE | GPIOH | PH5 | `DBG_LED_RED` |
| BLEU | GPIOE | PE10 | `DBG_LED_BLUE` |
| BLANC | GPIOE | PE13 | `DBG_LED_WHITE` |

**Checkpoints de boot** (séquence visuelle, session 8 — 2026-04-06) :
- **VERT** ON immédiat = FSBL démarré (PD0, CMSIS direct dans USER CODE 1, **avant** `HAL_Init`). Reste ON pendant toute la séquence FSBL. Puis extinction après delay 500ms dans USER CODE 2 + trace `[1/3 FSBL] LED VERTE`.
- **JAUNE** ON 500ms = AppliSecure atteint (PE9, trace `[2/3 AppliSecure] LED JAUNE` via CMSIS direct) — puis **OFF**
- **ROUGE** ON 500ms = AppliSecure phase 2 (PH5, trace `[2/3 AppliSecure] LED ROUGE`) — puis **OFF**
- **BLANC** ON = AppliNonSecure `main()` atteint (PE13, CMSIS direct avant `HAL_Init`)
- Après init complète : **chenillard** des 5 LEDs (une à la fois, 150 ms) + trace `[3/3 AppliNonSecure] Chenillard`

Toutes les broches LED sont déclarées **NSEC+NPRIV** dans AppliSecure (USER CODE RIF_Init 2).

**Note USART2 multi-firmware** : FSBL initialise USART2 via `HAL_UART_Init` (HAL_UART compilé dans FSBL). AppliSecure réutilise USART2 via écriture directe CMSIS (`boot_trace_putc` : poll `ISR.TXE_TXFNF`, write `TDR`) car `HAL_UART_MODULE_ENABLED` est commenté dans son `hal_conf.h`. Les registres USART2 persistent entre les jumps.

---

## 7. Debug série — USART2

| Paramètre | Valeur |
|-----------|--------|
| Périphérique | USART2 |
| Baud rate | 115200 |
| Format | 8N1 |
| TX | PD5, AF7 |
| RX | PF6, AF7 |
| Handle | `huart2` (global) |
| Init | `MX_USART2_UART_Init()` (CubeMX) |

**Fonctions de debug** :
- `DBG_Print(const char *s)` — envoie une chaîne via `HAL_UART_Transmit`
- `DBG_PrintDec(uint32_t v)` — affiche un entier en décimal
- `DBG_PrintHex32(uint32_t v)` — affiche un entier 32 bits en hexadécimal (0x…)

**TrustZone** : USART2 est ouvert en NSEC+NPRIV via `HAL_RIF_RISC_SetSlaveSecureAttributes(USART2, NSEC|NPRIV)` dans AppliSecure (USER CODE RIF_Init 2).

**Messages de boot (Phase 1 — confirmés fonctionnels)** :
```
=== PHASE 1 : FSBL LCD TEST ===
[OK] Security_Config
[OK] SRAM3+SRAM4 enabled
[OK] LTDC clock = 25 MHz
[LTDC] HAL_LTDC_Init OK
[LTDC] Layer 0 configured
[LTDC] Framebuffer filled RED
=== LCD should show RED ===
```

---

## 8. Toolchains disponibles

### 8.1 CubeIDE 1.11.0 (ancienne toolchain — remplacée par CubeIDE 2.1.1)

| Outil | Version | Chemin |
|-------|---------|--------|
| GCC | 10.3-2021.10 | `C:\Program Files\STM32CubeIDE_1.11.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.10.3-2021.10.win32_1.0.200.202301161003\tools\bin\` |
| Make | 2.0 | `...\externaltools.make.win32_2.0.100.202202231230\tools\bin\` |
| GDB server | 7.1 | **NE SUPPORTE PAS le STM32N6** (errno 10061) |
| CubeProgrammer | v2.20.0 (installé séparément) | `C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\` |

### 8.2 CubeIDE 2.1.1 (découvert session 11 — GDB fonctionnel)

Dossier : `C:\ST\STM32CubeIDE_1.13.0\STM32CubeIDE\` (nom trompeur, `stm32cubeide.ini` confirme `Version 2.1.1`, Build 28236, Mars 2026)

| Outil | Version | Chemin (relatif au dossier plugins) |
|-------|---------|--------|
| GCC | **14.3.1** | `...gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\` |
| Make | 2.2 | `...make.win32_2.2.100.202601091506\` |
| GDB server | **7.13.0** | `...stlink-gdb-server.win32_2.2.400.202601091506\tools\bin\` |
| CubeProgrammer | **v2.22.0** (bundled) | `...cubeprogrammer.win32_2.2.400.202601091506\tools\bin\` |

### 8.3 GDB debug — configuration fonctionnelle

**GDB server 7.13** fonctionne avec le STM32N6. Les paramètres clés :

```powershell
# Lancer le GDB server (en background)
ST-LINK_gdbserver.exe -p 61234 -l 31 \
  -cp "<chemin_cubeprogrammer_v2.22>" \
  -d -m 1 -k -i 004200253234511233353533 -g
```

| Paramètre | Valeur | Pourquoi |
|-----------|--------|----------|
| `-m 1` | AP ID = 1 | Cortex-M55 Secure debug (AP 0 = mauvais, donne PC=0x0) |
| `-k` | Connect under reset | Requis pour N6 TrustZone |
| `-g` | Attach | S'attache au code déjà en mémoire |
| `-d` | SWD mode | Interface debug |
| `-cp` | CubeProgrammer **v2.22.0 bundled** | Doit matcher le GDB server (v2.20.0 = Fail create session) |
| Fréquence | **Auto (24 MHz)** | `--freq` remplacé par `--frequency` dans v7.13 |
| `-i` | Serial number | `--serial` remplacé par `-i` dans v7.13 |

**GDB client** (test batch) :
```powershell
arm-none-eabi-gdb.exe --batch \
  -ex "target remote localhost:61234" \
  -ex "monitor halt" \
  -ex "info reg pc sp lr" \
  -ex "bt" \
  -ex "disconnect" -ex "quit" \
  FSBL/Debug/first_FSBL.elf
```

**Résultat confirmé** : session GDB créée (HALT_MODE), registres lus (PC dans le code FSBL, SP en SRAM3), backtrace obtenu. Le GDB server log montre `GdbSessionManager, session started: 1` puis `session terminated: 1` proprement.

**Attention** : les adresses lues via GDB sont en alias Secure `0x18xxxxxx` (= `0x34xxxxxx` - offset). Les symboles ELF compilés pour `0x34180000` ne matchent pas directement.

### 8.4 Pièges découverts (session 11)

1. **CubeIDE ouvert = ST-LINK bloqué** : `stm32cubeide.exe` (PID visible via `Get-Process`) prend le contrôle exclusif du ST-LINK. Les commandes CLI semblent réussir (`Start operation achieved successfully`) mais **rien ne se passe** sur la carte. Toujours fermer CubeIDE.
2. **`--freq` → `--frequency`** : le GDB server 7.13 a changé la syntaxe. `--freq` provoque un PARSE ERROR silencieux suivi de `Fail create session`.
3. **`--serial` → `-i`** : même changement de syntaxe dans v7.13.
4. **AP 0 vs AP 1** : `-m 0` (défaut) lit le mauvais Access Port → `PC: 0x0` → `Fail starting session`. Le fichier `.launch` de CubeIDE indique `access_port_id = 1`.
5. **CubeProgrammer v2.20 vs v2.22** : le GDB server 7.13 attend la version bundled v2.22. Avec v2.20 → `Fail create session`.

---

## 9. Migration IAR

**Statut** : planifiée, **en attente** que la Phase 2 (TrustZone + LVGL) fonctionne. La Phase 1 LCD fonctionne sous VS Code + GCC 14.3.1 (CubeIDE 2.1.1) + STM32_Programmer_CLI. Le debug GDB fonctionne avec GDB server 7.13. IntelliSense VS Code fonctionne via `compile_commands.json`. L'urgence migration IAR est réduite.

**Motivation** : certification IEC 62304 classe B (MDR 2017/745), compilateur pré-qualifié IEC 61508 SIL 3, meilleur debugger (C-SPY), support TrustZone natif.

**Prérequis** :
- IAR EWARM 9.60.4 avec Device Support Package STM32N6
- J-Link recommandé comme sonde principale
- CubeMX standalone gardé pour horloges / GPIO reference

**Plan** :
1. Changer toolchain CubeMX → EWARM, régénérer .eww/.ewp/.icf
2. Vérifier les 3 .icf (mêmes adresses que les .ld actuels)
3. Porter le code custom (sources HAL+CMSIS identiques, macros CMSIS portables)
4. Debug setup multi-image (FSBL + AppliSecure + AppliNonSecure)

---

## 10. Fichiers à lire en priorité

Pour un nouvel assistant, lire dans cet ordre :

| # | Fichier | Pourquoi |
|---|---------|----------|
| # | Fichier | Pourquoi |
|---|---------|----------|
| 1 | `ETAT_PROJET_FIRST.md` | Ce document — vue d'ensemble complète |
| 2 | `FSBL/Core/Src/main.c` | Phase 2a boot chain : Security, SRAM, USART2, Jump_To_AppliSecure |
| 3 | `AppliSecure/Core/Src/main.c` | **Code principal actif Phase 2** : TrustZone RIF, LTDC init Secure (section 7), GPIO/RIMC diagnostics |
| 4 | `FSBL/Core/Inc/stm32n6xx_hal_conf.h` | Modules HAL activés (LTDC, RAMCFG, RIF) |
| 5 | `FSBL/STM32N657X0HXQ_AXISRAM2_fsbl.ld` | Linker FSBL : ROM=0x34180400, RAM=0x341C0000 |
| 6 | `.vscode/tasks.json` | Tâches Build + Flash (3-step sequence) |
| 7 | `.vscode/c_cpp_properties.json` | 3 configurations IntelliSense + `compileCommands` |
| 8 | `compile_commands.json` | 904 entrées auto-générées via `make -nB` (FSBL+AppliSecure+AppliNonSecure) |
| 9 | `AppliNonSecure/Core/Src/main.c` | Application LVGL + chenillard LEDs + diagnostic LTDC |
| 10 | `AppliNonSecure/GUI/lvgl_port/lv_port_disp_ltdc.c` | Driver LTDC LVGL (Phase 2) |

**Autres fichiers .md existants** (contexte historique détaillé, non indispensables si ce document est lu) :
- `ANALYSE_PROJET_FIRST.md` — structure du projet CubeMX
- `DIAGNOSTIC_HARD_FAULT.md` — guide debug Hard Faults + SRAM shutdown
- `INTEGRATION_LVGL.md` — plan d'intégration LVGL initial
