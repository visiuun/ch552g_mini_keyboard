#pragma once
#include <stdint.h>

// Standard color hue presets (0..191)
#define NEO_RED 0
#define NEO_YEL 32
#define NEO_GREEN 64
#define NEO_CYAN 96
#define NEO_BLUE 128
#define NEO_MAG 160
#define NEO_WHITE 191
#define NEO_BRIGHT_KEYS 2

enum led_keyboard_mode_t
{
  LED_OFF,
  LED_FIX,
  LED_BREATHE,
  LED_LOOP
};

#ifdef __cplusplus
extern "C" {
#endif

// Mode & basic control
void led_set_mode(enum led_keyboard_mode_t mode);
void led_set_color_hue(uint8_t led0, uint8_t led1, uint8_t led2);
void led_update(void);
void led_presskey(int key);

// VMacropad Live USB Control APIs
void led_set_vmacropad_rgb(uint8_t r0, uint8_t g0, uint8_t b0,
                           uint8_t r1, uint8_t g1, uint8_t b1,
                           uint8_t r2, uint8_t g2, uint8_t b2);
void led_set_vmacropad_single(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
void led_set_vmacropad_mode(uint8_t mode);

#ifdef __cplusplus
}
#endif
