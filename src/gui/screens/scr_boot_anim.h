#ifndef SCR_BOOT_ANIM_H
#define SCR_BOOT_ANIM_H

#ifdef GUI_ENABLED

/* Play the boot animation (blocking, ~2.5s).
   Checks appState.bootAnimEnabled; does nothing if disabled. */
void boot_anim_play(void);

#endif /* GUI_ENABLED */
#endif /* SCR_BOOT_ANIM_H */
