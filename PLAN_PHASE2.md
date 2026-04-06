# Plan Phase 2 : Validation chaîne TrustZone + LVGL

**Date** : Session 13
**Prérequis validé** : Phase 1 — FSBL LCD RED fonctionnel (PLL4/24=25MHz, FB@0x34200000)

---

## État actuel

| Projet | .elf existant | Code | Exécuté ? |
|--------|:---:|------|:---:|
| **FSBL** | ✅ 57 KB | LCD RED + LED blink (Phase 1) | ✅ |
| **AppliSecure** | ✅ ~6 KB | RIF/RISAF + LTDC init + jump NS | ❌ |
| **AppliNonSecure** | ✅ ~400 KB | LVGL + chenillard 5 LEDs | ❌ |

Le FSBL boucle dans `while(1)` sans jamais sauter vers AppliSecure.

---

## 12 étapes de validation

### Phase 2a — FSBL → AppliSecure (sans LCD)

#### Étape 1 : tasks.json — Build + Flash 3 projets
- Ajouter tâches : `AppliSecure: Build`, `AppliNonSecure: Build`, `All: Build`
- Ajouter tâche flash 3 ELF : `All: Load to RAM + Run`
- Ordre flash : FSBL → AppliSecure → AppliNonSecure → start FSBL
- **Critère** : les 3 compilent, la tâche flash charge les 3 ELF

#### Étape 2 : FSBL — Saut vers AppliSecure
- USER CODE BEGIN 2 : retirer `LTDC_ClockConfig()` + `LTDC_Init_Phase1()`
- Garder : `USART2_Init`, `Security_Config`, `SRAM_Enable`, LED VERT
- Ajouter : UART_Print "[1/3 FSBL] → AppliSecure", saut @0x34000400
- Le `while(1)` ne sera plus atteint
- **Critère** : LED VERT → LED JAUNE → LED ROUGE (AppliSecure user code 2)

#### Étape 3 : Validation FSBL → AppliSecure
- Flash les 3 ELF, vérifier trace USART2 complète
- **Critère** : `[1/3 FSBL]` puis `[2/3 AppliSecure] LED JAUNE`
- **Git commit** : "Phase 2a: FSBL jumps to AppliSecure"

---

### Phase 2b — Chaîne complète sans LCD

#### Étape 4 : AppliNonSecure — Désactiver LVGL temporairement
- Commenter `lv_port_init()` + `lv_timer_handler()`
- Garder : `DBG_Board_Init`, `DBG_Print`, chenillard, Hello World USART2
- **Critère** : compile sans erreur

#### Étape 5 : Validation chaîne triple
- Flash 3 ELF, vérifier séquence complète
- **Critère** : LED VERT → JAUNE → ROUGE → BLANC (PE13 early diag NS)
  puis chenillard 5 LEDs + Hello World sur USART2
- **Git commit** : "Phase 2b: Full TZ chain (no LCD)"

---

### Phase 2c — LTDC dans la chaîne TrustZone

#### Étape 6 : AppliSecure — Pixel clock PLL4/24 au lieu de PLL1/45
- Le FSBL configure PLL4@600MHz dans SystemClock_Config → déjà actif
- Dans AppliSecure RIF_Init 2, section LTDC clock :
  - Remplacer `PLL1/45 = 26.67 MHz` par `PLL4/24 = 25 MHz`
  - Utiliser HAL_RCCEx_PeriphCLKConfig ou CMSIS (IC16CFGR source = PLL4)
- **Critère** : le registre IC16CFGR pointe sur PLL4, diviseur = 24

#### Étape 7 : FSBL — Retirer tout le code LCD
- Retirer `LTDC_ClockConfig`, `LTDC_Init_Phase1`, `HAL_LTDC_MspInit` du FSBL
- Retirer les includes `stm32n6xx_hal_ltdc.h` et la variable `hltdc`
- Retirer les defines LCD_WIDTH/HEIGHT/HSYNC etc.
- Le FSBL ne fait plus que : clocks, security, SRAM, USART, saut
- **Critère** : écran BLANC après boot (AppliSecure remplit FB avec 0xFFFF)

#### Étape 8 : AppliNonSecure — Test écriture FB
- Après `DBG_Board_Init`, écrire du BLEU (0x001F) dans `0x24200000`
- Si écran passe de blanc à bleu → NS écrit dans FB, LTDC actif
- **Critère** : écran BLEU + chenillard + trace USART
- **Git commit** : "Phase 2c: LTDC via TZ chain, NS FB write OK"

#### Étape 9 : Test couleurs cycliques (optionnel)
- Dans while(1) : changer couleur toutes les 2s (ROUGE/VERT/BLEU)
- Confirme que les écritures NS continues sont vues par LTDC
- **Critère** : couleurs changent sans freeze

---

### Phase 2d — LVGL

#### Étape 10 : Réactiver `lv_port_init()`
- Décommenter `lv_port_init()` + `lv_timer_handler()`
- `lv_port_disp_init()` crée le display LVGL sans toucher au LTDC matériel
- **Critère** : fond gris LVGL (thème par défaut), pas de crash

#### Étape 11 : LVGL Hello World
```c
lv_obj_t *label = lv_label_create(lv_screen_active());
lv_label_set_text(label, "Hello STM32N6!");
lv_obj_center(label);
```
- **Critère** : texte centré visible sur le LCD
- **Git commit** : "Phase 2d: LVGL Hello World"

#### Étape 12 : Nettoyage final
- Retirer le test couleur de l'étape 9
- Mettre à jour README.md + mémoire repo
- **Git commit** : "LVGL integration complete"

---

## Points critiques (leçons apprises)

| Piège | Cause | Remède |
|-------|-------|--------|
| Écran noir (RC#1) | PB13 (LCD_CLK) pas NSEC | AppliSecure ouvre PB13 dans RIF_Init 2 |
| Écran noir (RC#2) | RISAF4 PRIVC vs RIMC NPRIV | RISAF4 sans PRIVC, RIMC LTDC=NPRIV |
| Pixel clock muet | IC16CFGR = registre Secure-only | Configurer depuis AppliSecure uniquement |
| LCD_DE noir | PG13 en AF14 au lieu de GPIO OUT | GPIO_MODE_OUTPUT_PP + HIGH |
| SRAM3 en shutdown | SRAMSD=1 par défaut au reset | RAMCFG clear dans FSBL ou AppliSecure |
| Registres LTDC = 0 | GRMSK bloque le reload global | Utiliser per-layer RCR ou GRMSK=0 + SRCR.IMR |
| NS écritures RCC ignorées | Registres RCC Secure-only | Tout config horloge depuis Secure |
