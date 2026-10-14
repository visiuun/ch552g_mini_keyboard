#include <Arduino.h>
#include "neo/neo.h"
#include "led.h"

// Active LED mode: defaults to FIX (static color) - NO RAINBOW ON BOOT!
static enum led_keyboard_mode_t led_mode_s = LED_FIX;

// Current RGB colors for the 3 keys [R, G, B]
static uint8_t led_rgb_s[3][3] = {
    {0, 240, 255},  // Key 1: Cyan
    {255, 0, 127},  // Key 2: Pink / Magenta
    {118, 255, 3}   // Key 3: Lime Green
};

static uint8_t rainbow_hue_s[3] = {0, 64, 128};
static int breathe_val = 100;
static int8_t breathe_dir = 2;

void led_set_color_hue(uint8_t led0, uint8_t led1, uint8_t led2)
{
  rainbow_hue_s[0] = led0;
  rainbow_hue_s[1] = led1;
  rainbow_hue_s[2] = led2;
  led_mode_s = LED_LOOP;
}

void led_set_mode(enum led_keyboard_mode_t mode)
{
  led_mode_s = mode;
  if (mode == LED_OFF) {
    NEO_clearAll();
    NEO_update();
  }
}

// Keypress indicator: disabled so keys no longer flash blinding white
void led_presskey(int key)
{
  (void)key;
}

// Called directly from USB receiver when VMacropad sends 3-key RGB data (0xB1)
void led_set_vmacropad_rgb(uint8_t r0, uint8_t g0, uint8_t b0,
                           uint8_t r1, uint8_t g1, uint8_t b1,
                           uint8_t r2, uint8_t g2, uint8_t b2)
{
  led_rgb_s[0][0] = r0; led_rgb_s[0][1] = g0; led_rgb_s[0][2] = b0;
  led_rgb_s[1][0] = r1; led_rgb_s[1][1] = g1; led_rgb_s[1][2] = b1;
  led_rgb_s[2][0] = r2; led_rgb_s[2][1] = g2; led_rgb_s[2][2] = b2;
  led_mode_s = LED_FIX;
}

// Called when VMacropad sends single-key RGB data (0xB2)
void led_set_vmacropad_single(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
  if (idx < 3) {
    led_rgb_s[idx][0] = r;
    led_rgb_s[idx][1] = g;
    led_rgb_s[idx][2] = b;
    led_mode_s = LED_FIX;
  }
}

// Called when VMacropad sets LED mode (0xB0): 0=Off, 1=Static, 2=Breathing, 3=Rainbow
void led_set_vmacropad_mode(uint8_t mode)
{
  if (mode == 0) led_mode_s = LED_OFF;
  else if (mode == 2) led_mode_s = LED_BREATHE;
  else if (mode == 3) led_mode_s = LED_LOOP;
  else led_mode_s = LED_FIX;
}

void led_update()
{
  if (led_mode_s == LED_OFF)
  {
    NEO_clearAll();
    NEO_update();
    return;
  }

  if (led_mode_s == LED_LOOP)
  {
    for (int i = 0; i < 3; i++)
    {
      rainbow_hue_s[i] += 1;
      if (rainbow_hue_s[i] > 191) rainbow_hue_s[i] = 0;
      NEO_writeHue(i, rainbow_hue_s[i], NEO_BRIGHT_KEYS);
    }
    NEO_update();
    return;
  }

  if (led_mode_s == LED_BREATHE)
  {
    breathe_val += breathe_dir;
    if (breathe_val >= 100) { breathe_val = 100; breathe_dir = -2; }
    else if (breathe_val <= 10) { breathe_val = 10; breathe_dir = 2; }

    for (int i = 0; i < 3; i++)
    {
      uint8_t r = (uint16_t)led_rgb_s[i][0] * breathe_val / 100;
      uint8_t g = (uint16_t)led_rgb_s[i][1] * breathe_val / 100;
      uint8_t b = (uint16_t)led_rgb_s[i][2] * breathe_val / 100;
      NEO_writeColor(i, r, g, b);
    }
    NEO_update();
    return;
  }

  // LED_FIX: Render exact RGB values sent by VMacropad
  for (int led = 0; led < 3; led++)
  {
    NEO_writeColor(led, led_rgb_s[led][0], led_rgb_s[led][1], led_rgb_s[led][2]);
  }
  NEO_update();
}
