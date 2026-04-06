# État du projet « first » — STM32N6570-DK

**Dernière mise à jour : 2026-04-06 (session 11)**

## ÉCRAN LCD ROUGE — FONCTIONNEL ✅ | GDB DEBUG — FONCTIONNEL ✅

**Dernières évolutions (session 11 du 2026-04-06) :**
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
| **first_AppliSecure** | Inactif (pas de jump) | Config TrustZone — sera réactivé en Phase 2 |
| **first_AppliNonSecure** | Inactif (pas de jump) | Application LVGL — sera réactivé en Phase 2 |

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

### Ancienne architecture TrustZone (Phase 2 — à réactiver)

La chaîne FSBL → AppliSecure → AppliNonSecure fonctionnait pour les LEDs et USART2, mais le LCD restait noir en mode TrustZone split. Les écritures NS vers les registres LTDC étaient silencieusement ignorées. Phase 2 réintégrera TrustZone en gardant l'init LTDC côté Secure.

---

## 2. TrustZone / RIF — Configuration de sécurité

### 2.1 Phase 1 actuelle : FSBL Secure-only (`FSBL/Core/Src/main.c`)

En Phase 1, toute la config sécurité est dans le FSBL, via les API HAL :

- **RIMC** (Master attributes) : `HAL_RIF_RIMC_ConfigMasterAttributes` pour LTDC1 et LTDC2 = CID1 + SEC + PRIV
- **RISC** (Slave attributes) : `HAL_RIF_RISC_SetSlaveSecureAttributes` pour LTDC, LTDC_L1, LTDC_L2 = SEC + PRIV
- **SRAM3+SRAM4** : `HAL_RAMCFG_EnableAXISRAM` (sort du shutdown, active les clocks)
- **Pixel clock** : IC16 = PLL4 (600 MHz) / 24 = 25 MHz via `HAL_RCCEx_PeriphCLKConfig`
- **LTDC** : `HAL_LTDC_Init` + `HAL_LTDC_ConfigLayer` (tout via HAL, pas CMSIS direct)

### 2.2 Phase 2 (à venir) : AppliSecure + AppliNonSecure

Le code TrustZone split reste dans `AppliSecure/Core/Src/main.c` (USER CODE RIF_Init 2) mais n'est plus exécuté en Phase 1. Il contient :

- **RISUP NSEC+NPRIV** : USART2, LTDC, LTDC_L1, LTDC_L2
- **RIMC** : LTDC1/LTDC2 = CID1, NSEC, NPRIV (bit 31 MDMA forcé via CMSIS)
- **GPIO NSEC** : toutes les broches LCD (PA15, PB4, PB13, PE1, PG0 ajoutées manuellement car CubeMX les oublie), les 5 LEDs debug (PD0, PE9, PH5, PE10, PE13), GPIOQ (PQ3, PQ6)
- **SRAM3/4** : clocks + shutdown (retry si FSBL n'a pas pris effet)
- **RISAF4** : même config que FSBL (Region 0, ouvert à tous CIDs)
- **LTDC clocks** : IC16, CCIPR4, APB5, reset (mêmes valeurs que FSBL)
- **PUBCFGR** : IC16PUB, PERPUB, APB5PUB, AXISRAM3/4PUB
- **MPU NS** : region 7, 0x24200000 (768 KB), NON_CACHEABLE, OUTER_SHAREABLE, ALL_RW
- **Section 7 — LTDC FULL INIT FROM SECURE** (ajouté session 2026-04-05 soir) :
  - GPIO AF14 pour toutes les broches LCD (identique au BSP `LTDC_MspInit`)
  - Sorties PP : PQ3 (LCD_ON) HIGH, PQ6 (BL) HIGH, PG13 (DE, pull-up) HIGH
  - PE1 : cycle reset panneau (LOW 2ms → HIGH 120ms via `HAL_Delay`)
  - LTDC registres via `LTDC_S` / `LTDC_Layer1_S` : timings RK050HR18, layer 1 RGB565, `CFBAR=0x24200000`, `CFBLR`, `CFBLNR`, `CACR=0xFF`, `BFCR` PAxCA
  - Remplissage FB blanc (`0xFFFF`) via pointeur volatile sur 0x24200000
  - Enable (`LEN` + `LTDCEN`) + **`SRCR_IMR`** (reload immédiat, **sans GRMSK**)
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

**SAU (partition_stm32n657xx.h)** :
- Region 0 : NS SRAM2 (code+data) 0x24100000 – 0x241FFFFF
- Region 1 : NS SRAM3+SRAM4 (framebuffer) 0x24200000 – 0x242DFFFF
- Region 2 : NSC veneer 0x34001100 – 0x3400117F
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

### 4.4 LTDC init détaillée (maintenant dans `AppliSecure/Core/Src/main.c`, section 7)

Le driver LTDC est piloté par registres CMSIS via les alias **Secure** (`LTDC_S`, `LTDC_Layer1_S`) car :
1. STM32N6 ne fournit pas de HAL LTDC fonctionnelle (`stm32n6xx_hal_ltdc.c` est un stub vide)
2. Les écritures NS vers `LTDC_NS` sont **silencieusement ignorées** (voir §2.3)

Points clés :
- **RCR = 0** : ne **JAMAIS** mettre **GRMSK** (bit 2). GRMSK masque le reload global (`SRCR`) → les shadow registers ne se chargent pas → registres actifs = valeurs reset (0). C'était la cause du CFBAR=0.
- **Reload** : utiliser `LTDC_SRCR_IMR` (reload immédiat global), attendre que le HW efface le bit.
- **CFBLR** : `line_length = width * bytes_per_pixel + 7` (et non +3 comme les anciens STM32).
- **Séquence** : `LEN` → `LTDCEN` → `RCR=0` → `SRCR=IMR` → attente.
- **Framebuffer** : rempli en blanc (0xFFFF RGB565) par Secure avant enable.

---

## 5. LCD / LTDC — FONCTIONNEL ✅

### 5.1 État actuel

**L'écran affiche du ROUGE** (RGB565 0xF800) en mode FSBL Secure-only (Phase 1). Confirmé par trace USART et visuellement.

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

**Leçon clé** : sur STM32N6, le LCD doit être initialisé **entièrement côté Secure**. Les écritures NS vers LTDC sont silencieusement ignorées, même avec RISUP NSEC. L'approche HAL (pas CMSIS direct) est plus fiable et reproductible.

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

### 8.1 CubeIDE 1.11.0 (toolchain actuelle pour le build)

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

**Statut** : planifiée, **en attente** que la Phase 2 (TrustZone + LVGL) fonctionne. La Phase 1 LCD fonctionne sous VS Code + GCC + STM32_Programmer_CLI. Le debug GDB fonctionne avec CubeIDE 2.1.1 (GDB server 7.13), ce qui réduit l'urgence de la migration IAR.

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
| 1 | `ETAT_PROJET_FIRST.md` | Ce document — vue d'ensemble complète |
| 2 | `FSBL/Core/Src/main.c` | **Code principal actif** : Phase 1 LCD test, Security, SRAM, LTDC HAL, LED blink |
| 3 | `FSBL/Core/Inc/stm32n6xx_hal_conf.h` | Modules HAL activés (LTDC, RAMCFG, RIF) |
| 4 | `FSBL/STM32N657X0HXQ_AXISRAM2_fsbl.ld` | Linker FSBL : ROM=0x34180400, RAM=0x341C0000 |
| 5 | `.vscode/tasks.json` | Tâches Build + Flash (3-step sequence) |
| 6 | `AppliSecure/Core/Src/main.c` | Config TrustZone (Phase 2, inactif) |
| 7 | `AppliNonSecure/Core/Src/main.c` | Application LVGL (Phase 2, inactif) |
| 8 | `AppliNonSecure/GUI/lvgl_port/lv_port_disp_ltdc.c` | Driver LTDC LVGL (Phase 2, inactif) |

**Autres fichiers .md existants** (contexte historique détaillé, non indispensables si ce document est lu) :
- `ANALYSE_PROJET_FIRST.md` — structure du projet CubeMX
- `DIAGNOSTIC_HARD_FAULT.md` — guide debug Hard Faults + SRAM shutdown
- `INTEGRATION_LVGL.md` — plan d'intégration LVGL initial
