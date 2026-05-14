/* PChat Audio Player Plugin - GUI Interface
 * Copyright (C) 2025
 */

#ifndef AUDIOPLAYER_GUI_H
#define AUDIOPLAYER_GUI_H

#include "audioplayer.h"
#include "../../src/common/pchat-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize and show the GUI window */
void audioplayer_gui_init(pchat_plugin *ph, AudioPlayer *player);

/* Update GUI state (called from plugin when state changes) */
void audioplayer_gui_update(void);

/* Cleanup GUI resources */
void audioplayer_gui_cleanup(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AUDIOPLAYER_GUI_H */
