#pragma once

// call these from your project's setup/loop hooks
void mpr121_setup();
void mpr121_loop();

// optional app-level helpers you will implement later
void yoradio_save_favorite(uint8_t btn);
void yoradio_goto_favorite(uint8_t btn);