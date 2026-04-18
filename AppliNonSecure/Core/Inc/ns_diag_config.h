#ifndef NS_DIAG_CONFIG_H
#define NS_DIAG_CONFIG_H

/**
 * @brief 1 = pas d'init LVGL ni lv_timer_handler : affichage uniquement via
 *        SECURE_LtdcFbFillRgb565 (NSC). Utile pour isoler acces NS -> FB.
 *        0 = comportement applicatif normal (LVGL).
 */
#define NS_DIAG_NO_LVGL  1

#endif /* NS_DIAG_CONFIG_H */
