#pragma once
#include <stdint.h>

/* Windows-style wallpaper fit modes. */
enum { BANNER_FIT_FILL, BANNER_FIT_FIT, BANNER_FIT_STRETCH, BANNER_FIT_CENTER, BANNER_FIT_TILE };
void banner_load(const char *path);                 /* stretch (system art) */
void banner_load_fit(const char *path, int mode, uint16_t bg);  /* wallpaper */
/* Reuse the native decoder with a temporary budget for Enhanced assets. */
unsigned char *banner_decode_enhanced_image(const unsigned char *encoded, int size,
                                            int width, int height, int channels);
/* Enhanced images are decoded and bounded before submission. On success
 * the banner owns the RGB buffer and releases it after conversion. */
int banner_enhanced_current(const char *id, int mode, uint16_t bg);
int banner_load_enhanced(const char *id, unsigned char *rgb, int width, int height,
                         int mode, uint16_t bg);
void banner_clear_enhanced(void);
void banner_clear(void);
void banner_render(uint16_t *framebuffer);
/* Draw the currently loaded artwork into a contained card without decoding it
 * again. */
void banner_draw_card(uint16_t *framebuffer, int x, int y, int w, int h);
int  banner_is_loaded(void);
int  banner_is_animating(void);
void banner_set_anim(int enabled);
void banner_prefetch_clear(void);
void banner_prefetch(const char *path, int mode, uint16_t bg);
void banner_set_dim(int percent);
void banner_fill_region(uint16_t *fb, int x, int y, int w, int h, uint16_t fallback);
