# État du projet « first » — STM32N6570-DK

**Dernière mise à jour : 2026-04-05**

Ce document centralise toute la connaissance acquise sur le projet. Il permet à un assistant sans historique de reprendre le travail immédiatement.

---

## 1. Chaîne de boot (TrustZone Cortex-M55)

Le STM32N657 intègre TrustZone. Le projet se compose de 3 binaires compilés séparément :

| Étape | Projet | Adresse VTOR | Rôle |
|-------|--------|-------------|------|
| 1 | **first_FSBL** | 0x34180400 | First Stage Bootloader. Init horloges (PLL1 1200 MHz), XSPI, SDMMC, GPIO, mémoires externes. Configure SRAM3/4, RISAF, RIMC, LTDC clocks, GPIO NSEC pour LCD et LEDs. Saute vers AppliSecure à 0x34000400. |
| 2 | **first_AppliSecure** | 0x34000400 | Monde sécurisé. Init HAL, configure RIFSC (RISUP NSEC pour USART2/LTDC), GPIO pin attributes (NSEC), RISAF4, RIMC LTDC, horloges pixel/APB5, PUBCFGR. Saute vers AppliNonSecure via `NonSecure_Init()`. |
| 3 | **first_AppliNonSecure** | 0x24100400 | Monde non-sécurisé. Application principale : HAL_Init, GPIO, USART2, LCD power, LTDC GPIO, diagnostics, LVGL init, boucle `lv_timer_handler()` + chenillard LEDs + Hello World série. |

**Comment flasher** : dans STM32CubeIDE, sélectionner le projet **first_FSBL** puis **Run** (ou **Debug**) via `first_FSBL.launch`. Ce lanceur charge les 3 ELF (FSBL + AppliSecure + AppliNonSecure) en une passe.

---

## 2. TrustZone / RIF — Configuration de sécurité

### 2.1 FSBL (`FSBL/Core/Src/main.c`, USER CODE BEGIN 2)

Le FSBL effectue la configuration hardware critique **avant** de sauter vers AppliSecure :

- **SRAM3/4** : clocks ON (`RCC->MEMENSR`), sortie shutdown (`RAMCFG_SRAM3/4_AXI_S->CR`, clear `SRAMSD`)
- **RISAF4** (protège SRAM3+4, NPU port 0) : Region 0, `STARTR=0x24200000`, `ENDR=0x242DFFFF`, `CIDCFGR=0x00FF00FF` (tous CIDs R/W), `CFGR=BREN` (NSEC, NPRIV)
- **RISAF2** (protège SRAM1) : Region 0, `0x00000000`–`0x000FFFFF` (1 MB), mêmes ouvertures
- **RIMC** : LTDC L1 (index 10) et L2 (index 11) = CID1, NSEC, NPRIV, MDMA
- **RISUP (LTDC)** : `RISC_SECCFGRx[3]` bits 6-8 cleared (LTDC/L1/L2 = NSEC+NPRIV)
- **GPIO NSEC** : PB13 (LCD_CLK), PE1 (LCD_NRST), PG0 (LCD_R0), PQ3 (LCD_ON), PQ6 (LCD_BL)
- **LTDC clocks** : IC16 = PLL1/45 = 26.67 MHz, CCIPR4 LTDCSEL=IC16, APB5 LTDC enable + reset
- **PUBCFGR** : AXISRAM3PUB, AXISRAM4PUB, IC16PUB, PERPUB, APB5PUB
- **Saut** : MSP + jump à `0x34000400` (AppliSecure)

### 2.2 AppliSecure (`AppliSecure/Core/Src/main.c`, USER CODE RIF_Init 2)

AppliSecure redouble certaines configs (ceinture + bretelles) et ajoute :

- **RISUP NSEC+NPRIV** : USART2, LTDC, LTDC_L1, LTDC_L2
- **RIMC** : LTDC1/LTDC2 = CID1, NSEC, NPRIV
- **GPIO NSEC** : toutes les broches LCD (PA15, PB4, PB13, PE1, PG0 ajoutées manuellement car CubeMX les oublie), les 5 LEDs debug (PD0, PE9, PH5, PE10, PE13), GPIOQ (PQ3, PQ6)
- **SRAM3/4** : clocks + shutdown (retry si FSBL n'a pas pris effet)
- **RISAF4** : même config que FSBL (Region 0, ouvert à tous CIDs)
- **LTDC clocks** : IC16, CCIPR4, APB5, reset (mêmes valeurs que FSBL)
- **PUBCFGR** : IC16PUB, PERPUB, APB5PUB, AXISRAM3/4PUB
- **NonSecure_Init()** : écrit `VTOR_NS = SRAM2_AXI_BASE_NS + 0x400`, positionne MSP_NS, saute vers le Reset_Handler NS

### 2.3 Points critiques TrustZone (leçons apprises)

- Les registres `RCC->IC16CFGR`, `DIVENSR`, `CCIPR4`, `APB5ENSR` sont **Secure-only** : les écritures NS sont **silencieusement ignorées**.
- PB13 (LCD_CLK) doit être **NSEC** dans AppliSecure, sinon le `HAL_GPIO_Init(AF14)` NS est silencieusement ignoré → pas de pixel clock → écran noir.
- Le STM32N6 RCC utilise des **SET registers** (`xxxENSR`) pour activer les clocks : écrire dans `xxxENR` directement ne fonctionne **pas**.
- `RCC_PRIVCFGR5` n'existe pas sur le STM32N6 (PRIVCFGR0..4 uniquement). Seul `PUBCFGR5` existe.

---

## 3. Memory map (linker)

| Région | Origine | Taille | Usage |
|--------|---------|--------|-------|
| **ROM** (NS) | 0x24100400 | 511K | Code + constantes AppliNonSecure (SRAM2) |
| **RAM** (NS) | 0x24180000 | 512K | Data + BSS + heap (4K) + stack (8K) |
| **FBRAM** | 0x24010000 | 768K | Framebuffer LTDC (section `.framebuffer`, SRAM1) |
| **Secure ROM** | 0x34000400 | 399K | AppliSecure |
| **Secure RAM** | 0x34064000 | 624K | AppliSecure data |
| **FSBL ROM** | 0x34180400 | 255K | FSBL |

**SAU (partition_stm32n657xx.h)** :
- Region 0 : NS SRAM2 (code+data) 0x24100000 – 0x241FFFFF
- Region 1 : NS SRAM3+SRAM4 (framebuffer) 0x24200000 – 0x242DFFFF
- Region 2 : NSC veneer 0x34001100 – 0x3400117F
- Region 3 : NS peripherals 0x40000000 – 0x4FFFFFFF

**Note** : le framebuffer a été déplacé de SRAM3 (0x24200000) vers SRAM1 (0x24010000) dans le linker pour éviter les problèmes RISAF4/NPU. Mais la section SAU Region 1 couvre toujours SRAM3/4. Le code `lv_port_disp_ltdc.c` place le buffer via `__attribute__((section(".framebuffer")))` → FBRAM.

---

## 4. LVGL — État d'intégration

### 4.1 Sources et configuration

| Élément | Chemin | État |
|---------|--------|------|
| Bibliothèque LVGL | `Middlewares/lvgl/` | Copiée depuis `C:\Develop\libs\lvgl` |
| Config LVGL | `AppliNonSecure/Core/Inc/lv_conf.h` | Configuré : `LV_COLOR_DEPTH 16`, `LV_MEM_SIZE 64K`, `LV_USE_ST_LTDC 0` (LTDC piloté en CMSIS direct), widgets de base activés, démos désactivées |
| Port display | `AppliNonSecure/GUI/lvgl_port/lv_port_disp_ltdc.c` | Init LTDC CMSIS directe, framebuffer 800×480 RGB565 en section `.framebuffer`, mode partial (20 lignes, double buffer render) |
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
/* Après HAL_Init + MX_GPIO_Init + MX_USART2_UART_Init + LCD power : */
lv_port_init();  /* → lv_init() + ltdc_hw_init() + lv_display_create() */

/* Boucle principale : */
while(1) {
    lv_timer_handler();  /* LVGL task handler */
    /* + chenillard + Hello World */
}
```

Un bouton "Hello LVGL!" centré est créé dans le code avant la boucle.

### 4.4 LTDC init détaillée (`lv_port_disp_ltdc.c`)

Le driver LTDC est piloté par registres CMSIS (`LTDC_NS`, `LTDC_Layer1_NS`) car **STM32N6 ne fournit pas de HAL LTDC** (`stm32n6xx_hal_ltdc.c` n'existe pas dans le firmware pack). Points clés :

- **Layer RCR** : utilise le reload per-layer `RCR = IMR | GRMSK (0x05)`. Ne jamais utiliser le reload global `SRCR.IMR` ni effacer GRMSK (provoque la mise à zéro de tous les registres layer).
- **CFBLR** : `line_length = width * bytes_per_pixel + 7` (et non +3 comme les anciens STM32).
- **Séquence** : le LTDC est activé (`GCR |= LTDCEN`) **avant** le reload layer (conforme au HAL ST).

---

## 5. LCD / LTDC — État et blocage actuel

### 5.1 Symptôme

**L'écran reste noir** malgré de nombreux correctifs appliqués au cours de 4 sessions de debug.

### 5.2 Ce qui fonctionne (confirmé)

- Le LTDC **scanne** : le registre CPSR (Current Position Status) change entre deux lectures → le LTDC est actif et parcourt l'image.
- Le **fond bleu** (BCCR = 0x000000FF, layer 1 désactivé) est **visible** → les signaux LTDC atteignent physiquement le panneau LCD et les GPIOs fonctionnent.
- Les registres layer relus sont corrects : CR=0x01, WHPCR, WVPCR, PFCR=0x02 (RGB565), CACR=0xFF, CFBAR, CFBLR, CFBLNR = valeurs attendues.
- Le test SRAM3 W/R passe (write 0xBEEF, read 0xBEEF).
- Toutes les horloges sont actives (MEMENR, AHB2ENR, AHB3ENR, APB5ENR vérifiés avec diagnostics USART2).
- Les readbacks GPIO PB13 (AF14), PG13 (OUTPUT HIGH), PE1 (OUTPUT HIGH) sont corrects.
- RISAF4 IASR = 0 (pas d'accès illégal détecté).

### 5.3 Causes racines déjà trouvées et corrigées

| # | Cause | Statut |
|---|-------|--------|
| 1 | **PB13 (LCD_CLK) pas NSEC** : CubeMX n'avait pas généré l'attribut NSEC. Le `HAL_GPIO_Init(AF14)` NS était silencieusement ignoré → pas de pixel clock → écran noir. | **Corrigé** dans AppliSecure + FSBL |
| 2 | **RISAF4 privilege mismatch** : CFGR avait PRIVC1 set (CID1 requiert PRIV), RIMC configure LTDC comme NPRIV → tout accès DMA refusé → pixels noirs. | **Corrigé** : CIDCFGR=0x00FF00FF, CFGR=BREN seul |
| 3 | **LCD_DE (PG13) en AF14** au lieu de GPIO OUTPUT PP HIGH : le LTDC mettait DE à LOW pendant le blanking → panneau interprétait "display OFF". | **Corrigé** : configuré en GPIO OUTPUT HIGH |
| 4 | **SRAM3/4 en shutdown** (SRAMSD=1 au reset) : accès = BusFault. | **Corrigé** dans FSBL + AppliSecure |

### 5.4 Hypothèses restantes (non résolues — écran toujours noir avec layer ON)

1. **RISAF2 pourrait CASSER l'accès SRAM1** — SRAM1/2 sont peut-être ouverts par défaut (contrairement à SRAM3-6). Ajouter une région RISAF2 pourrait restreindre ce qui était précédemment ouvert. **Hypothèse** : essayer de SUPPRIMER la config RISAF2 du FSBL.
2. **Framebuffer read-back test** — après remplissage avec 0xF800 (rouge), relire depuis NS. Si 0 → RISAF bloque les lectures NS. Si 0xF800 → c'est le DMA LTDC spécifiquement qui est bloqué.
3. **RIMC/CID vérification** — vérifier que le CID DMA LTDC correspond effectivement à ce que RISAF2 autorise.
4. **Alias d'adresse** — LTDC configuré NSEC (RIMC) accède 0x24010000 (alias NS). Si RISAF2 ne filtre que l'alias S (0x34010000), mismatch.
5. **Pixel clock** : 26.67 MHz (PLL1/45) vs BSP 25 MHz (PLL4/2). Peu probable mais à tester.
6. **Essayer framebuffer en SRAM2** — RISAF3 est déjà configuré par CubeMX pour NS. Placer le framebuffer en fin de SRAM2 (0x241C0000) où il y a de la place.

### 5.5 Historique des sessions LCD

| Session | Action | Résultat |
|---------|--------|----------|
| 1 | Init LTDC basique, framebuffer SRAM3 | Noir |
| 2 | Per-layer reload, LCD_DE GPIO, LCD_NRST reset, PCPOL | Noir |
| 3 | PB13/PE1/PG0 NSEC, RISAF4 open, RIMC LTDC NSEC | **Bleu** (fond) / Noir (layer) |
| 4 | Déplacement FB vers SRAM1, RISAF2 config | Noir (layer) |

**Blocage actuel** : le LTDC output fonctionne (fond bleu visible), mais le DMA LTDC ne parvient pas à lire le framebuffer en SRAM. Le problème est entre RISAF/RIMC et l'accès DMA.

---

## 6. Debug matériel — LEDs

5 LEDs soudées sur une carte de debug externe, initialisées par `DBG_Board_Init()` dans `AppliNonSecure/Core/Src/main.c`.

| Couleur | Port | Pin | Macro |
|---------|------|-----|-------|
| VERT | GPIOD | PD0 | `DBG_LED_GREEN` |
| JAUNE | GPIOE | PE9 | `DBG_LED_YELLOW` |
| ROUGE | GPIOH | PH5 | `DBG_LED_RED` |
| BLEU | GPIOE | PE10 | `DBG_LED_BLUE` |
| BLANC | GPIOE | PE13 | `DBG_LED_WHITE` |

**Checkpoints de boot** (allumage séquentiel au démarrage) :
- **VERT** ON = `main()` NS atteint (via CMSIS direct, avant `HAL_Init`)
- **JAUNE** ON = `HAL_Init()` passé
- Après init complète : **chenillard** des 5 LEDs (une à la fois, 150 ms par LED)

Toutes les broches LED sont déclarées **NSEC+NPRIV** dans AppliSecure (USER CODE RIF_Init 2).

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

**TrustZone** : USART2 est ouvert en NSEC+NPRIV via `HAL_RIF_RISC_SetSlaveSecureAttributes(USART2, NSEC|NPRIV)` dans AppliSecure (USER CODE RIF_Init 2).

**Messages de boot typiques** :
```
=== STM32N657 NS BOOT ===
[1] HAL+GPIO+UART OK
[2] LCD power...
[3] LCD power OK
[4] LTDC GPIO OK
[4b] GPIO readback: PB13 AF14 OK, PG13 OUTPUT HIGH OK, PE1 OUTPUT HIGH OK
[5] DIAG START (clocks, RISAF dump, SRAM W/R test, framebuffer fill)
[6] LVGL init...
[7] LVGL init OK
[BG TEST] Layer OFF + BCCR=BLEU... (5s observation)
Hello World #1, #2, #3...
```

---

## 8. Migration IAR

**Statut** : planifiée, **en attente** que l'écran LCD fonctionne sous CubeIDE.

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

## 9. Fichiers à lire en priorité

Pour un nouvel assistant, lire dans cet ordre :

| # | Fichier | Pourquoi |
|---|---------|----------|
| 1 | `ETAT_PROJET_FIRST.md` | Ce document — vue d'ensemble complète |
| 2 | `AppliNonSecure/Core/Src/main.c` | Application principale, diagnostics, LVGL init, boucle |
| 3 | `AppliNonSecure/GUI/lvgl_port/lv_port_disp_ltdc.c` | Driver LTDC CMSIS + LVGL display |
| 4 | `AppliSecure/Core/Src/main.c` | Config TrustZone (RIFSC, RIMC, GPIO NSEC, clocks) |
| 5 | `FSBL/Core/Src/main.c` | Boot, SRAM init, RISAF, RIMC, LTDC clocks, saut AppliSecure |
| 6 | `AppliNonSecure/Core/Inc/lv_conf.h` | Config LVGL |
| 7 | `AppliNonSecure/STM32N657X0HXQ_LRUN.ld` | Linker : adresses ROM/RAM/FBRAM |
| 8 | `AppliNonSecure/Core/Inc/main.h` | Defines pins LCD, macros |

**Autres fichiers .md existants** (contexte historique détaillé, non indispensables si ce document est lu) :
- `ANALYSE_PROJET_FIRST.md` — structure du projet CubeMX
- `DIAGNOSTIC_HARD_FAULT.md` — guide debug Hard Faults + SRAM shutdown
- `INTEGRATION_LVGL.md` — plan d'intégration LVGL initial
