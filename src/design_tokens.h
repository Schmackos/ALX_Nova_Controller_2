#ifndef DESIGN_TOKENS_H
#define DESIGN_TOKENS_H

/* ====== Design Tokens — Single Source of Truth ======
 * Both LVGL (gui_theme.h) and Web UI CSS use these values.
 * When updating a color, change it here and in the corresponding
 * CSS variables (see cross-reference comments).
 */

/* --- Accent --- */
#define DT_ACCENT          0xFF9800  /* CSS: --accent */
#define DT_ACCENT_LIGHT    0xFFB74D  /* CSS: --accent-light */
#define DT_ACCENT_DARK     0xE68900  /* CSS: --accent-dark */

/* --- Dark Mode Backgrounds (Neutral) --- */
#define DT_DARK_BG         0x121212  /* CSS: --bg-primary (night) */
#define DT_DARK_CARD       0x1E1E1E  /* CSS: --bg-surface (night) */
#define DT_DARK_SURFACE    0x2A2A2A  /* LVGL focus highlight */
#define DT_DARK_INPUT      0x252525  /* CSS: --bg-card (night) */
#define DT_DARK_INPUT2     0x2C2C2C  /* CSS: --bg-input (night) */
#define DT_DARK_BORDER     0x333333  /* CSS: --border (night) */

/* --- Light Mode Backgrounds --- */
#define DT_LIGHT_BG        0xF5F5F5  /* CSS: --bg-primary */
#define DT_LIGHT_CARD      0xFFFFFF  /* CSS: --bg-surface */
#define DT_LIGHT_SURFACE   0xE0E0E0  /* LVGL focus highlight */
#define DT_LIGHT_CARD2     0xEEEEEE  /* CSS: --bg-card */
#define DT_LIGHT_INPUT     0xE0E0E0  /* CSS: --bg-input */
#define DT_LIGHT_BORDER    0xE0E0E0  /* CSS: --border */

/* --- Text (Dark Mode) --- */
#define DT_TEXT_PRIMARY     0xFFFFFF  /* CSS: --text-primary (night) */
#define DT_TEXT_SECONDARY   0xB0B0B0  /* CSS: --text-secondary (night) */
#define DT_TEXT_DISABLED    0x666666  /* CSS: --text-disabled (night) */

/* --- Text (Light Mode) --- */
#define DT_TEXT_PRIMARY_LT  0x212121  /* CSS: --text-primary */
#define DT_TEXT_SEC_LT      0x757575  /* CSS: --text-secondary */

/* --- Status Colors --- */
#define DT_SUCCESS          0x4CAF50  /* CSS: --success */
#define DT_WARNING          0xFFC107  /* CSS: --warning */
#define DT_ERROR            0xF44336  /* CSS: --error */
#define DT_INFO             0x2196F3  /* CSS: --info */

#endif /* DESIGN_TOKENS_H */
