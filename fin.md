# Fin de session — mise à jour de `ETAT_PROJET_FIRST.md`

## Comment l’utiliser (toi, humain)

Dans **Cursor**, **VS Code** ou tout autre IDE avec assistant, tu peux écrire par exemple :

- *« Lis `fin.md` et exécute le prompt qu’il contient pour mettre à jour `ETAT_PROJET_FIRST.md`. »*
- ou *« Applique la procédure de fin de session décrite dans `fin.md`. »*

L’assistant doit **lire ce fichier**, puis **modifier** `ETAT_PROJET_FIRST.md` (même dossier racine du projet `first`) selon le bloc **PROMPT ASSISTANT** ci-dessous — en s’appuyant sur **l’historique de la session en cours** et sur le **code du dépôt** si besoin.

---

## PROMPT ASSISTANT (à exécuter quand l’utilisateur demande la fin de session)

Tu es dans le dépôt du projet **first** (STM32N6570-DK, TrustZone). Ta tâche est de **mettre à jour** le fichier **`ETAT_PROJET_FIRST.md`** à la racine du projet pour qu’un assistant **sans historique de chat** puisse reprendre le travail.

### Entrées

1. Lis **`ETAT_PROJET_FIRST.md`** dans son intégralité (structure actuelle à préserver).
2. Utilise **cette conversation / session** comme source principale des changements depuis la dernière mise à jour.
3. Si la session a modifié le hardware, le linker, RIF/LTDC, LVGL ou les chemins de fichiers, **vérifie rapidement** les fichiers concernés (grep ou lecture ciblée) pour ne pas documenter une information fausse.

### Règles

- **Met à jour** la ligne **`Dernière mise à jour : YYYY-MM-DD`** avec la date du jour (année selon l’environnement utilisateur si fournie).
- **Ne réécris pas tout le document from scratch** : ajuste uniquement les sections impactées par la session, en gardant le même plan numéroté (1 à 9).
- Pour tout ce que tu n’as **pas** vérifié dans le repo : écris *« non vérifié cette session »* ou laisse l’ancien texte inchangé.
- Distingue clairement **fait confirmé**, **changement dans le code**, et **hypothèse**.
- Si une section n’a pas bougé, tu peux la laisser telle quelle (pas de bruit inutile).

### Contenu minimal à refléter après la session

Ajoute ou mets à jour au moins une des formes suivantes (selon ce qui s’est passé) :

1. **Résumé court** (2–8 phrases) en tête du document, **après** la ligne de date : *« Dernières évolutions (session du …) : … »* — ou intègre ce résumé dans la section la plus pertinente (ex. §5 LCD) si tu préfères éviter de dupliquer.
2. **§4 LVGL / §3 Memory map** : si adresses, `.ld`, `lv_conf`, ou fichiers de port ont changé.
3. **§5 LCD / LTDC** : symptômes, tests faits, hypothèses **validées ou invalidées**, prochain test recommandé.
4. **§2 TrustZone / RIF** : seulement si la session a touché FSBL, AppliSecure, RISAF, RIMC, GPIO NSEC, etc.
5. **§6 LEDs / §7 USART2** : seulement si broches, checkpoints ou fonctions de debug ont changé.
6. **§8 Migration IAR** : seulement si le statut ou les prérequis ont changé.
7. **§9 Fichiers prioritaires** : seulement si de nouveaux fichiers deviennent centraux.

### Livrable

- Un commit logique côté fichier : **`ETAT_PROJET_FIRST.md`** à jour, cohérent, sans contradictions internes évidentes.
- En fin de ta réponse à l’utilisateur : liste **en une phrase** les sections que tu as modifiées (ex. *« §5.4–5.5 et date »*).

---

## Note

Ce fichier **`fin.md`** ne remplace pas **`ETAT_PROJET_FIRST.md`** : il ne contient que la **procédure** et le **prompt** pour le maintenir. Pour démarrer une session dans un autre IDE, lire d’abord **`ETAT_PROJET_FIRST.md`**.
